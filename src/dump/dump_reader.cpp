/**
 * @file dump_reader.cpp
 * @brief Implementation of efficient Wikimedia dump reader with index support
 */

#include "wikilib/dump/dump_reader.h"
#include <algorithm>
#include <bzlib.h>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <pugixml.hpp>
#include <stdexcept>
#include <sys/stat.h>
#include "wikilib/dump/bz2_line_reader.h"
#include "wikilib/dump/bz2_stream.h"
#include "wikilib/dump/index_chunker.h"

namespace wikilib::dump {

namespace {

bool parse_xml_chunk(pugi::xml_document &doc, std::string_view xml, std::string &error) {
    constexpr auto flags = pugi::parse_default | pugi::parse_fragment | pugi::parse_ws_pcdata;
    auto result = doc.load_buffer(xml.data(), xml.size(), flags);
    if (result)
        return true;
    // The first and last indexed streams may contain only one side of the
    // mediawiki root. Complete that boundary without accepting partial pages.
    for (const auto &completed: {std::string(xml) + "</mediawiki>", "<mediawiki>" + std::string(xml)}) {
        result = doc.load_buffer(completed.data(), completed.size(), flags);
        if (result)
            return true;
    }
    error = std::string("XML chunk parse failed: ") + result.description();
    return false;
}

std::optional<std::string> find_page_content(const std::string &title, const std::string &xml, std::string &error) {
    pugi::xml_document doc;
    if (!parse_xml_chunk(doc, xml, error))
        return std::nullopt;
    pugi::xml_node root = doc.child("mediawiki");
    if (!root)
        root = doc;
    for (auto page: root.children("page")) {
        if (title == page.child_value("title")) {
            return std::string(page.child("revision").child("text").text().as_string());
        }
    }
    return std::nullopt;
}

} // namespace

// ============================================================================
// DumpReader implementation
// ============================================================================

struct DumpReader::Impl {
    DumpPath path;
    std::string error_message;

    // Index data
    bool index_is_loaded = false;
    std::unordered_map<std::string, IndexedPage> page_map;
    std::vector<uint64_t> chunk_offsets; // Start offset for each chunk

    // File handle for dump
    FILE *dump_file = nullptr;

    explicit Impl(const DumpPath &p) : path(p) {
    }

    ~Impl() {
        if (dump_file) {
            fclose(dump_file);
        }
    }

    // Open dump file
    bool open_dump();

    // Buffered decompression of a complete range
    std::string decompress_range(uint64_t start, uint64_t length);
};

bool DumpReader::Impl::open_dump() {
    if (dump_file) {
        return true;
    }

    auto dump_path = path.dump_path();
    dump_file = fopen(dump_path.string().c_str(), "rb");
    if (!dump_file) {
        error_message = "Failed to open dump file: " + dump_path.string();
        return false;
    }
    return true;
}

std::string DumpReader::Impl::decompress_range(uint64_t start, uint64_t length) {
    const uint64_t size = path.dump_size();
    if (length == 0 || start >= size || length > size - start ||
        start > static_cast<uint64_t>(std::numeric_limits<long>::max()) ||
        length > std::numeric_limits<unsigned int>::max()) {
        error_message = "Invalid or unsupported compressed range";
        return {};
    }
    if (!open_dump()) {
        return {};
    }

    // Seek to start position
    if (fseek(dump_file, static_cast<long>(start), SEEK_SET) != 0) {
        error_message = "Seek failed";
        return {};
    }

    // Read compressed data
    std::vector<char> compressed(length);
    size_t bytes_read = fread(compressed.data(), 1, length, dump_file);
    if (bytes_read != length) {
        error_message = "Failed to read compressed data";
        return {};
    }

    // Buffered decompression with adaptive output capacity
    // Start with reasonable estimate and grow if needed
    std::string result;
    unsigned int dest_len = static_cast<unsigned int>(std::min<uint64_t>(length * 15, 1024 * 1024));
    result.resize(dest_len);

    int ret = BZ2_bzBuffToBuffDecompress(result.data(), &dest_len, compressed.data(), static_cast<unsigned int>(length),
                                         0, // small
                                         0 // verbosity
    );

    // If buffer too small, retry with larger buffer
    while (ret == BZ_OUTBUFF_FULL) {
        if (result.size() == std::numeric_limits<unsigned int>::max()) {
            error_message = "Decompressed chunk exceeds BZ2 buffer API limit";
            return {};
        }
        dest_len = static_cast<unsigned int>(
                std::min<uint64_t>(static_cast<uint64_t>(result.size()) * 2, std::numeric_limits<unsigned int>::max()));
        result.resize(dest_len);
        ret = BZ2_bzBuffToBuffDecompress(result.data(), &dest_len, compressed.data(), static_cast<unsigned int>(length),
                                         0, 0);
    }

    if (ret != BZ_OK) {
        error_message = "BZ2 decompression failed";
        return {};
    }

    result.resize(dest_len);
    return result;
}

DumpReader::DumpReader(const DumpPath &path) : impl_(std::make_unique<Impl>(path)) {
}

DumpReader::~DumpReader() = default;

DumpReader::DumpReader(DumpReader &&) noexcept = default;
DumpReader &DumpReader::operator=(DumpReader &&) noexcept = default;

void DumpReader::load_index(std::function<void(size_t)> progress_callback) {
    impl_->error_message.clear();
    impl_->index_is_loaded = false;
    impl_->page_map.clear();
    impl_->chunk_offsets.clear();
    if (impl_->dump_file) {
        fclose(impl_->dump_file);
        impl_->dump_file = nullptr;
    }

    try {
        auto index_path = impl_->path.index_path();
        if (!std::filesystem::exists(index_path)) {
            auto plain_index = index_path;
            plain_index.replace_extension();
            if (std::filesystem::exists(plain_index))
                index_path = std::move(plain_index);
        }
        const uint64_t dump_size = impl_->path.dump_size();
        if (dump_size == 0)
            throw std::runtime_error("Dump file not found or empty");
        std::unordered_map<std::string, IndexedPage> pages;
        std::vector<uint64_t> offsets;
        IndexChunker chunker =
                IndexChunker::from_file(index_path.string(), dump_size, IndexLinePolicy::RejectMalformed);

        IndexChunk chunk;
        size_t chunk_idx = 0;

        // Progress tracking - update every 1000 chunks OR every 2 seconds
        auto last_progress_time = std::chrono::steady_clock::now();
        constexpr size_t CHUNK_INTERVAL = 1000;
        constexpr auto TIME_INTERVAL = std::chrono::seconds(2);

        while (chunker.next_chunk(chunk)) {
            // Store chunk offset
            offsets.push_back(chunk.start_offset);

            // Store page entries
            for (const auto &entry: chunk.entries) {
                IndexedPage page;
                page.id = entry.page_id;
                page.title = entry.title;
                page.chunk_index = offsets.size() - 1;
                if (!pages.emplace(entry.title, std::move(page)).second)
                    throw std::runtime_error("Duplicate page title in index: " + entry.title);
            }

            chunk_idx++;

            // Progress callback: every N chunks or every M seconds
            if (progress_callback) {
                auto now = std::chrono::steady_clock::now();
                bool time_elapsed = (now - last_progress_time) >= TIME_INTERVAL;
                bool chunk_interval = (chunk_idx % CHUNK_INTERVAL == 0);

                if (time_elapsed || chunk_interval) {
                    progress_callback(chunk_idx);
                    last_progress_time = now;
                }
            }
        }
        if (!chunker.error().empty())
            throw std::runtime_error(std::string(chunker.error()));
        if (pages.empty())
            throw std::runtime_error("No valid entries found in index file");

        // Add final offset (EOF)
        offsets.push_back(dump_size);

        // Final progress update
        if (progress_callback) {
            progress_callback(chunk_idx);
        }
        impl_->page_map = std::move(pages);
        impl_->chunk_offsets = std::move(offsets);
        impl_->index_is_loaded = true;
    } catch (const std::exception &e) {
        impl_->index_is_loaded = false;
        impl_->page_map.clear();
        impl_->chunk_offsets.clear();
        impl_->error_message = std::string("Failed to load index: ") + e.what();
    }
}

bool DumpReader::index_loaded() const noexcept {
    return impl_->index_is_loaded;
}

size_t DumpReader::page_count() const noexcept {
    return impl_->page_map.size();
}

size_t DumpReader::chunk_count() const noexcept {
    return impl_->chunk_offsets.empty() ? 0 : impl_->chunk_offsets.size() - 1;
}

bool DumpReader::has_page(const std::string &title) const {
    return impl_->page_map.contains(title);
}

std::optional<IndexedPage> DumpReader::get_page_info(const std::string &title) const {
    auto it = impl_->page_map.find(title);
    if (it == impl_->page_map.end()) {
        return std::nullopt;
    }
    return it->second;
}

ExtractedPage DumpReader::extract_page(const std::string &title) {
    impl_->error_message.clear();
    ExtractedPage result;
    result.title = title;
    if (!impl_->index_is_loaded) {
        impl_->error_message = "Index not loaded";
        return result;
    }

    auto info = get_page_info(title);
    if (!info) {
        return result;
    }

    // Decompress the chunk
    std::string xml = decompress_chunk(info->chunk_index);
    if (xml.empty()) {
        return result;
    }

    // Extract page from XML
    auto content = find_page_content(title, xml, impl_->error_message);
    if (content)
        result.content = std::move(*content);
    else if (impl_->error_message.empty())
        impl_->error_message = "Indexed page missing from XML chunk: " + title;
    result.id = info->id;
    result.found = content.has_value();

    return result;
}

std::vector<ExtractedPage> DumpReader::extract_pages(const std::vector<std::string> &titles) {
    impl_->error_message.clear();
    std::vector<ExtractedPage> results;
    results.reserve(titles.size());

    // Group titles by chunk for efficient extraction
    std::unordered_map<size_t, std::vector<size_t>> by_chunk;

    for (const auto &title: titles) {
        auto info = get_page_info(title);
        if (info) {
            by_chunk[info->chunk_index].push_back(results.size());
        }

        ExtractedPage page;
        page.title = title;
        results.push_back(std::move(page));
    }
    if (!impl_->index_is_loaded) {
        impl_->error_message = "Index not loaded";
        return results;
    }

    // Extract from each chunk
    std::string first_error;
    for (const auto &[chunk_idx, indices]: by_chunk) {
        std::string xml = decompress_chunk(chunk_idx);
        if (xml.empty()) {
            if (first_error.empty())
                first_error = impl_->error_message;
            continue;
        }

        for (const auto idx: indices) {
            const auto &title = results[idx].title;
            auto info = get_page_info(title);

            auto content = find_page_content(title, xml, impl_->error_message);
            if (content)
                results[idx].content = std::move(*content);
            else if (impl_->error_message.empty())
                impl_->error_message = "Indexed page missing from XML chunk: " + title;
            results[idx].id = info ? info->id : 0;
            results[idx].found = content.has_value();
            if (first_error.empty())
                first_error = impl_->error_message;
        }
    }
    impl_->error_message = std::move(first_error);

    return results;
}

std::string DumpReader::decompress_chunk(size_t chunk_idx) {
    impl_->error_message.clear();
    if (chunk_idx >= chunk_count()) {
        impl_->error_message = "Chunk index out of range";
        return {};
    }

    uint64_t start = impl_->chunk_offsets[chunk_idx];
    uint64_t end = impl_->chunk_offsets[chunk_idx + 1];

    return decompress_chunk(start, end - start);
}

std::string DumpReader::decompress_chunk(uint64_t start_offset, uint64_t length) {
    impl_->error_message.clear();
    try {
        return impl_->decompress_range(start_offset, length);
    } catch (const std::exception &error) {
        impl_->error_message = error.what();
        return {};
    }
}

void DumpReader::process_all(std::function<bool(const std::string &, const std::string &)> callback) {
    process_all(callback, nullptr);
}

void DumpReader::process_all(std::function<bool(const std::string &, const std::string &)> callback,
                             std::function<void(const ProcessProgress &)> progress) {
    auto dump_path_str = impl_->path.dump_path();
    uint64_t total_size = impl_->path.dump_size();

    Bz2Stream stream(dump_path_str.string());
    if (!stream.is_open()) {
        impl_->error_message = "Failed to open dump file";
        return;
    }

    // Progress tracking
    ProcessProgress prog;
    prog.bytes_total = total_size;
    auto last_progress_time = std::chrono::steady_clock::now();
    constexpr size_t PAGE_INTERVAL = 1000;
    constexpr auto TIME_INTERVAL = std::chrono::seconds(2);

    auto maybe_report_progress = [&]() {
        if (!progress)
            return;

        auto now = std::chrono::steady_clock::now();
        bool time_elapsed = (now - last_progress_time) >= TIME_INTERVAL;
        bool page_interval = (prog.pages_processed % PAGE_INTERVAL == 0);

        if (time_elapsed || page_interval) {
            prog.bytes_compressed = stream.compressed_bytes_read();
            progress(prog);
            last_progress_time = now;
        }
    };

    // Read and process XML
    std::string page_content;
    bool in_page = false;

    while (auto line = stream.read_line()) {
        const std::string &l = *line;

        // Simple state machine for page boundaries
        if (l.find("<page>") != std::string::npos) {
            in_page = true;
            page_content.clear();
        }

        if (in_page) {
            page_content += l;
            page_content += '\n';
        }

        if (l.find("</page>") != std::string::npos && in_page) {
            in_page = false;

            // Parse this page
            auto pages = extract_all_from_xml(page_content);
            for (const auto &[title, content]: pages) {
                prog.pages_processed++;
                maybe_report_progress();

                if (!callback(title, content)) {
                    // Final progress before exit
                    if (progress) {
                        prog.bytes_compressed = stream.compressed_bytes_read();
                        progress(prog);
                    }
                    return;
                }
            }
        }
    }

    // Final progress
    if (progress) {
        prog.bytes_compressed = stream.compressed_bytes_read();
        progress(prog);
    }
}

const DumpPath &DumpReader::path() const noexcept {
    return impl_->path;
}

const std::string &DumpReader::error() const noexcept {
    return impl_->error_message;
}

// ============================================================================
// XML parsing utilities
// ============================================================================

std::string extract_page_from_xml(const std::string &title, const std::string &xml_chunk) {
    std::string error;
    auto content = find_page_content(title, xml_chunk, error);
    return content.value_or("");
}

std::vector<std::pair<std::string, std::string>> extract_all_from_xml(const std::string &xml_chunk) {
    std::vector<std::pair<std::string, std::string>> result;
    std::string error;
    pugi::xml_document doc;
    if (!parse_xml_chunk(doc, xml_chunk, error))
        return result;
    pugi::xml_node root = doc.child("mediawiki");
    if (!root)
        root = doc;
    for (auto page: root.children("page")) {
        result.emplace_back(page.child_value("title"), page.child("revision").child("text").text().as_string());
    }
    return result;
}

} // namespace wikilib::dump
