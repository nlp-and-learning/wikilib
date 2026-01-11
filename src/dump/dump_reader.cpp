/**
 * @file dump_reader.cpp
 * @brief Implementation of efficient Wikimedia dump reader with index support
 */

#include "wikilib/dump/dump_reader.h"
#include "wikilib/dump/bz2_stream.h"
#include "wikilib/dump/bz2_line_reader.h"
#include "wikilib/dump/index_chunker.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <pugixml.hpp>
#include <sys/stat.h>
#include <bzlib.h>

namespace wikilib::dump {

// ============================================================================
// DumpReader implementation
// ============================================================================

struct DumpReader::Impl {
    DumpPath path;
    std::string error_message;

    // Index data
    bool index_is_loaded = false;
    std::unordered_map<std::string, IndexedPage> page_map;
    std::vector<uint64_t> chunk_offsets;  // Start offset for each chunk

    // File handle for dump
    FILE* dump_file = nullptr;

    explicit Impl(const DumpPath& p) : path(p) {}

    ~Impl() {
        if (dump_file) {
            fclose(dump_file);
        }
    }

    // Open dump file
    bool open_dump();

    // Streaming decompression of a range
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

    // Streaming decompression with adaptive buffer
    // Start with reasonable estimate and grow if needed
    std::string result;
    unsigned int dest_len = static_cast<unsigned int>(length * 15);
    result.resize(dest_len);

    int ret = BZ2_bzBuffToBuffDecompress(
        result.data(),
        &dest_len,
        compressed.data(),
        static_cast<unsigned int>(length),
        0,  // small
        0   // verbosity
    );

    // If buffer too small, retry with larger buffer
    while (ret == BZ_OUTBUFF_FULL) {
        dest_len *= 2;
        result.resize(dest_len);
        ret = BZ2_bzBuffToBuffDecompress(
            result.data(),
            &dest_len,
            compressed.data(),
            static_cast<unsigned int>(length),
            0, 0
        );
    }

    if (ret != BZ_OK) {
        error_message = "BZ2 decompression failed";
        return {};
    }

    result.resize(dest_len);
    return result;
}

DumpReader::DumpReader(const DumpPath& path)
    : impl_(std::make_unique<Impl>(path)) {
}

DumpReader::~DumpReader() = default;

DumpReader::DumpReader(DumpReader&&) noexcept = default;
DumpReader& DumpReader::operator=(DumpReader&&) noexcept = default;

void DumpReader::load_index(std::function<void(size_t)> progress_callback) {
    auto index_path = impl_->path.index_path();
    uint64_t dump_size = impl_->path.dump_size();

    if (dump_size == 0) {
        impl_->error_message = "Dump file not found or empty";
        return;
    }

    try {
        IndexChunker chunker = IndexChunker::from_file(
            index_path.string(),
            dump_size
        );

        IndexChunk chunk;
        size_t chunk_idx = 0;

        // Progress tracking - update every 1000 chunks OR every 2 seconds
        auto last_progress_time = std::chrono::steady_clock::now();
        constexpr size_t CHUNK_INTERVAL = 1000;
        constexpr auto TIME_INTERVAL = std::chrono::seconds(2);

        while (chunker.next_chunk(chunk)) {
            // Store chunk offset
            if (impl_->chunk_offsets.empty() ||
                impl_->chunk_offsets.back() != chunk.start_offset) {
                impl_->chunk_offsets.push_back(chunk.start_offset);
            }

            // Store page entries
            for (const auto& entry : chunk.entries) {
                IndexedPage page;
                page.id = entry.page_id;
                page.title = entry.title;
                page.chunk_index = impl_->chunk_offsets.size() - 1;

                impl_->page_map[entry.title] = std::move(page);
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

        // Add final offset (EOF)
        impl_->chunk_offsets.push_back(dump_size);

        impl_->index_is_loaded = true;

        // Final progress update
        if (progress_callback) {
            progress_callback(chunk_idx);
        }
    } catch (const std::exception& e) {
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

bool DumpReader::has_page(const std::string& title) const {
    return impl_->page_map.contains(title);
}

std::optional<IndexedPage> DumpReader::get_page_info(const std::string& title) const {
    auto it = impl_->page_map.find(title);
    if (it == impl_->page_map.end()) {
        return std::nullopt;
    }
    return it->second;
}

ExtractedPage DumpReader::extract_page(const std::string& title) {
    ExtractedPage result;
    result.title = title;

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
    result.content = extract_page_from_xml(title, xml);
    result.id = info->id;
    result.found = !result.content.empty();

    return result;
}

std::vector<ExtractedPage> DumpReader::extract_pages(const std::vector<std::string>& titles) {
    std::vector<ExtractedPage> results;
    results.reserve(titles.size());

    // Group titles by chunk for efficient extraction
    std::unordered_map<size_t, std::vector<std::string>> by_chunk;
    std::unordered_map<std::string, size_t> result_idx;

    for (const auto& title : titles) {
        auto info = get_page_info(title);
        if (info) {
            by_chunk[info->chunk_index].push_back(title);
        }
        result_idx[title] = results.size();

        ExtractedPage page;
        page.title = title;
        results.push_back(std::move(page));
    }

    // Extract from each chunk
    for (const auto& [chunk_idx, chunk_titles] : by_chunk) {
        std::string xml = decompress_chunk(chunk_idx);
        if (xml.empty()) continue;

        for (const auto& title : chunk_titles) {
            auto idx = result_idx[title];
            auto info = get_page_info(title);

            results[idx].content = extract_page_from_xml(title, xml);
            results[idx].id = info ? info->id : 0;
            results[idx].found = !results[idx].content.empty();
        }
    }

    return results;
}

std::string DumpReader::decompress_chunk(size_t chunk_idx) {
    if (chunk_idx >= chunk_count()) {
        impl_->error_message = "Chunk index out of range";
        return {};
    }

    uint64_t start = impl_->chunk_offsets[chunk_idx];
    uint64_t end = impl_->chunk_offsets[chunk_idx + 1];

    return impl_->decompress_range(start, end - start);
}

std::string DumpReader::decompress_chunk(uint64_t start_offset, uint64_t length) {
    return impl_->decompress_range(start_offset, length);
}

void DumpReader::process_all(std::function<bool(const std::string&, const std::string&)> callback) {
    process_all(callback, nullptr);
}

void DumpReader::process_all(
    std::function<bool(const std::string&, const std::string&)> callback,
    std::function<void(const ProcessProgress&)> progress
) {
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
        if (!progress) return;

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
        const std::string& l = *line;

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
            for (const auto& [title, content] : pages) {
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

const DumpPath& DumpReader::path() const noexcept {
    return impl_->path;
}

const std::string& DumpReader::error() const noexcept {
    return impl_->error_message;
}

// ============================================================================
// XML parsing utilities
// ============================================================================

std::string extract_page_from_xml(
    const std::string& title,
    const std::string& xml_chunk
) {
    // Wrap chunk in root element for valid XML
    std::string xml_str = "<mediawiki>\n" + xml_chunk + "</mediawiki>\n";

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml_str.c_str());

    if (!result) {
        return {};
    }

    for (pugi::xml_node page : doc.child("mediawiki").children("page")) {
        std::string page_title = page.child_value("title");
        if (page_title == title) {
            pugi::xml_node revision = page.child("revision");
            if (revision) {
                pugi::xml_node text_node = revision.child("text");
                if (text_node) {
                    return text_node.text().as_string();
                }
            }
            break;
        }
    }

    return {};
}

std::vector<std::pair<std::string, std::string>> extract_all_from_xml(
    const std::string& xml_chunk
) {
    std::vector<std::pair<std::string, std::string>> result;

    // Wrap chunk in root element for valid XML
    std::string xml_str = "<mediawiki>\n" + xml_chunk + "</mediawiki>\n";

    pugi::xml_document doc;
    pugi::xml_parse_result parse_result = doc.load_string(xml_str.c_str());

    if (!parse_result) {
        return result;
    }

    for (pugi::xml_node page : doc.child("mediawiki").children("page")) {
        std::string title = page.child_value("title");

        pugi::xml_node revision = page.child("revision");
        if (revision) {
            pugi::xml_node text_node = revision.child("text");
            if (text_node) {
                std::string content = text_node.text().as_string();
                result.emplace_back(std::move(title), std::move(content));
            }
        }
    }

    return result;
}

} // namespace wikilib::dump
