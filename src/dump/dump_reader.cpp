/**
 * @file dump_reader.cpp
 * @brief Implementation of efficient Wikimedia dump reader with index support
 */

#include "wikilib/dump/dump_reader.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <map>
#include <pugixml.hpp>
#include <stdexcept>
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

std::string latest_content(pugi::xml_node page) {
    pugi::xml_node latest;
    for (auto revision: page.children("revision"))
        latest = revision;
    std::string content;
    for (auto node: latest.child("text").children()) {
        if (node.type() == pugi::node_pcdata || node.type() == pugi::node_cdata)
            content += node.value();
    }
    return content;
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
            return latest_content(page);
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

    explicit Impl(const DumpPath &p) : path(p) {
    }

    // Buffered decompression of a complete range
    std::string decompress_range(uint64_t start, uint64_t length);
    bool process_range(const IndexChunk &chunk, const PageCallback &callback, ProcessProgress &progress,
                       const ProgressCallback &report, std::chrono::steady_clock::time_point &last_report);
    std::filesystem::path index_path() const;
};

std::filesystem::path DumpReader::Impl::index_path() const {
    auto index = path.index_path();
    if (!std::filesystem::exists(index)) {
        auto plain = index;
        plain.replace_extension();
        if (std::filesystem::exists(plain))
            return plain;
    }
    return index;
}

std::string DumpReader::Impl::decompress_range(uint64_t start, uint64_t length) {
    if (length == 0 || length > std::numeric_limits<uint64_t>::max() - start) {
        error_message = "Invalid compressed range";
        return {};
    }
    Bz2RangeReader range(path.dump_path().string(), start, start + length);
    std::string result;
    char buffer[64 * 1024];
    while (const auto count = range.read(buffer, sizeof(buffer)))
        result.append(buffer, count);
    if (!range.error().empty()) {
        error_message = range.error();
        return {};
    }
    return result;
}

bool DumpReader::Impl::process_range(const IndexChunk &chunk, const PageCallback &callback, ProcessProgress &progress,
                                     const ProgressCallback &report,
                                     std::chrono::steady_clock::time_point &last_report) {
    for (const auto &entry: chunk.entries) {
        if (entry.offset != chunk.start_offset) {
            error_message = "Index entry does not match chunk offset";
            return false;
        }
    }
    auto range = std::make_unique<Bz2RangeReader>(path.dump_path().string(), chunk.start_offset, chunk.end_offset);
    if (!range->is_open()) {
        error_message = range->error();
        return false;
    }
    const auto *range_info = range.get();
    const uint64_t bytes_before = progress.bytes_compressed;
    PageHandler handler(std::make_unique<XmlReader>(XmlReader::from_chunk(std::move(range))));
    size_t pages_in_chunk = 0;
    PageFilter filter;
    while (auto page = handler.next_page(filter)) {
        progress.bytes_compressed = bytes_before + range_info->compressed_bytes_read();
        if (!chunk.entries.empty() &&
            (pages_in_chunk >= chunk.entries.size() || page->info.title != chunk.entries[pages_in_chunk].title ||
             page->info.id != chunk.entries[pages_in_chunk].page_id)) {
            error_message = "XML page does not match index entry at chunk " + std::to_string(chunk.start_offset) +
                            ", position " + std::to_string(pages_in_chunk) +
                            ": XML id=" + std::to_string(page->info.id) + ", title=" + page->info.title;
            if (pages_in_chunk < chunk.entries.size())
                error_message += "; index id=" + std::to_string(chunk.entries[pages_in_chunk].page_id) +
                                 ", title=" + chunk.entries[pages_in_chunk].title;
            return false;
        }
        ++pages_in_chunk;
        ++progress.pages_processed;
        if (!callback(*page))
            return false;
        const auto now = std::chrono::steady_clock::now();
        if (report && (progress.pages_processed % 1000 == 0 || now - last_report >= std::chrono::seconds(2))) {
            report(progress);
            last_report = now;
        }
    }
    progress.bytes_compressed = bytes_before + range_info->compressed_bytes_read();
    if (!handler.error().empty()) {
        error_message = handler.error();
        return false;
    }
    if (!chunk.entries.empty() && pages_in_chunk != chunk.entries.size()) {
        error_message = "Indexed page missing from XML chunk";
        return false;
    }
    ++progress.chunks_processed;
    return true;
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
    try {
        auto index_path = impl_->index_path();
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
    return extract_pages({title}).front();
}

std::vector<ExtractedPage> DumpReader::extract_pages(const std::vector<std::string> &titles) {
    impl_->error_message.clear();
    std::vector<ExtractedPage> results;
    results.reserve(titles.size());
    // chunk indexes follow validated, increasing compressed offsets.
    std::map<size_t, std::vector<size_t>> by_chunk;
    for (const auto &title: titles) {
        if (auto info = get_page_info(title))
            by_chunk[info->chunk_index].push_back(results.size());
        ExtractedPage page;
        page.title = title;
        results.push_back(std::move(page));
    }
    if (!impl_->index_is_loaded) {
        impl_->error_message = "Index not loaded";
        return results;
    }

    std::string first_error;
    for (const auto &[chunk_idx, indices]: by_chunk) {
        std::unordered_map<std::string, ExtractedPage> selected;
        for (const auto idx: indices) {
            const auto &title = results[idx].title;
            selected.try_emplace(title, ExtractedPage{title, {}, impl_->page_map.at(title).id, false});
        }
        IndexChunk chunk;
        chunk.start_offset = impl_->chunk_offsets[chunk_idx];
        chunk.end_offset = impl_->chunk_offsets[chunk_idx + 1];
        ProcessProgress progress;
        auto last_report = std::chrono::steady_clock::now();
        impl_->error_message.clear();
        bool complete = false;
        try {
            complete = impl_->process_range(
                    chunk,
                    [&](const Page &page) {
                        auto found = selected.find(page.info.title);
                        if (found != selected.end()) {
                            found->second.content = page.content();
                            found->second.found = true;
                        }
                        return true;
                    },
                    progress, nullptr, last_report);
        } catch (const std::exception &error) {
            impl_->error_message = error.what();
        }
        // Publish results only after validating the entire chunk. A corrupt
        // suffix must not make an earlier requested page appear successful.
        if (!complete) {
            if (first_error.empty())
                first_error = impl_->error_message;
            continue;
        }
        for (const auto idx: indices) {
            results[idx] = selected.at(results[idx].title);
            if (!results[idx].found && first_error.empty())
                first_error = "Indexed page missing from XML chunk: " + results[idx].title;
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

bool DumpReader::process_chunk(const IndexChunk &chunk, PageCallback callback, ProgressCallback report) {
    impl_->error_message.clear();
    ProcessProgress progress;
    try {
        if (!callback)
            throw std::invalid_argument("Page callback is required");
        if (chunk.start_offset >= chunk.end_offset)
            throw std::invalid_argument("Invalid compressed range");
        progress.bytes_total = chunk.end_offset - chunk.start_offset;
        if (report)
            report(progress);
        auto last_report = std::chrono::steady_clock::now();
        const bool complete = impl_->process_range(chunk, callback, progress, report, last_report);
        if (report)
            report(progress);
        return complete;
    } catch (const std::exception &error) {
        impl_->error_message = error.what();
        return false;
    }
}

bool DumpReader::process_indexed(PageCallback callback, ProgressCallback report) {
    impl_->error_message.clear();
    ProcessProgress progress;
    try {
        if (!callback)
            throw std::invalid_argument("Page callback is required");
        const auto dump_size = impl_->path.dump_size();
        if (dump_size == 0)
            throw std::runtime_error("Dump file not found or empty");
        auto chunker =
                IndexChunker::from_file(impl_->index_path().string(), dump_size, IndexLinePolicy::RejectMalformed);
        IndexChunk chunk;
        bool has_chunk = chunker.next_chunk(chunk);
        if (!chunker.error().empty())
            throw std::runtime_error(std::string(chunker.error()));
        if (!has_chunk)
            throw std::runtime_error("No valid entries found in index file");
        progress.bytes_total = dump_size - chunk.start_offset;
        if (report)
            report(progress);
        auto last_report = std::chrono::steady_clock::now();
        while (has_chunk) {
            if (!impl_->process_range(chunk, callback, progress, report, last_report)) {
                if (report)
                    report(progress);
                return false;
            }
            has_chunk = chunker.next_chunk(chunk);
        }
        if (!chunker.error().empty()) {
            impl_->error_message = chunker.error();
            if (report)
                report(progress);
            return false;
        }
        if (report)
            report(progress);
        return true;
    } catch (const std::exception &error) {
        impl_->error_message = error.what();
        return false;
    }
}

void DumpReader::process_all(std::function<bool(const std::string &, const std::string &)> callback) {
    process_all(callback, nullptr);
}

void DumpReader::process_all(std::function<bool(const std::string &, const std::string &)> callback,
                             std::function<void(const ProcessProgress &)> progress) {
    impl_->error_message.clear();
    ProcessProgress prog;
    try {
        if (!callback)
            throw std::invalid_argument("Page callback is required");
        prog.bytes_total = impl_->path.dump_size();
        auto stream = std::make_unique<Bz2Stream>(impl_->path.dump_path().string());
        if (!stream->is_open())
            throw std::runtime_error(std::string(stream->error()));
        const auto *stream_info = stream.get();
        PageHandler handler(std::make_unique<XmlReader>(std::move(stream)));
        auto last_report = std::chrono::steady_clock::now();
        if (progress)
            progress(prog);
        PageFilter filter;
        while (auto page = handler.next_page(filter)) {
            prog.bytes_compressed = stream_info->compressed_bytes_read();
            ++prog.pages_processed;
            const std::string empty_content;
            const auto &content = page->revisions.empty() ? empty_content : page->revisions.back().content;
            if (!callback(page->info.title, content)) {
                if (progress)
                    progress(prog);
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            if (progress && (prog.pages_processed % 1000 == 0 || now - last_report >= std::chrono::seconds(2))) {
                progress(prog);
                last_report = now;
            }
        }
        prog.bytes_compressed = stream_info->compressed_bytes_read();
        impl_->error_message = handler.error();
        if (progress)
            progress(prog);
    } catch (const std::exception &error) {
        impl_->error_message = error.what();
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
        result.emplace_back(page.child_value("title"), latest_content(page));
    }
    return result;
}

} // namespace wikilib::dump
