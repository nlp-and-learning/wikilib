#pragma once

/**
 * @file dump_reader.h
 * @brief Efficient reader for Wikimedia dump files with index support
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "wikilib/dump/dump_path.h"
#include "wikilib/dump/index_chunker.h"

namespace wikilib::dump {

/**
 * @brief Entry in the indexed object map
 */
struct IndexedPage {
    PageId id = 0;
    std::string title;
    size_t chunk_index = 0;  // Index into chunk offset vector

    IndexedPage() = default;
    IndexedPage(PageId p_id, std::string p_title, size_t chunk_idx)
        : id(p_id), title(std::move(p_title)), chunk_index(chunk_idx) {}
};

/**
 * @brief Extracted page content
 */
struct ExtractedPage {
    std::string title;
    std::string content;
    PageId id = 0;
    bool found = false;
};

/**
 * @brief Efficient reader for Wikimedia dump files
 *
 * Uses index file to enable random access to compressed dump.
 * Supports streaming decompression with adaptive buffer sizing.
 *
 * Example usage:
 * @code
 *   DumpPath path("/path/to/wikimedia-dump-fetcher");
 *   path.set_project(WikiProject::Wiktionary).set_language("pl");
 *
 *   DumpReader reader(path);
 *   reader.load_index();  // Load index into memory
 *
 *   // Extract single page
 *   auto page = reader.extract_page("kot");
 *   if (page.found) {
 *       std::cout << page.content << std::endl;
 *   }
 *
 *   // Extract multiple pages efficiently
 *   auto pages = reader.extract_pages({"kot", "pies", "mysz"});
 * @endcode
 */
class DumpReader {
public:
    /**
     * @brief Create reader for dump at path
     */
    explicit DumpReader(const DumpPath& path);

    ~DumpReader();

    // Non-copyable, movable
    DumpReader(const DumpReader&) = delete;
    DumpReader& operator=(const DumpReader&) = delete;
    DumpReader(DumpReader&&) noexcept;
    DumpReader& operator=(DumpReader&&) noexcept;

    /**
     * @brief Load index into memory
     *
     * Reads the entire index file and builds lookup tables.
     * Progress callback is called periodically.
     *
     * @param progress_callback Optional callback for progress (chunks processed)
     */
    void load_index(std::function<void(size_t)> progress_callback = nullptr);

    /**
     * @brief Check if index is loaded
     */
    [[nodiscard]] bool index_loaded() const noexcept;

    /**
     * @brief Get number of indexed pages
     */
    [[nodiscard]] size_t page_count() const noexcept;

    /**
     * @brief Get number of chunks
     */
    [[nodiscard]] size_t chunk_count() const noexcept;

    /**
     * @brief Check if a page exists in the index
     */
    [[nodiscard]] bool has_page(const std::string& title) const;

    /**
     * @brief Get indexed page info
     */
    [[nodiscard]] std::optional<IndexedPage> get_page_info(const std::string& title) const;

    /**
     * @brief Extract a single page by title
     */
    [[nodiscard]] ExtractedPage extract_page(const std::string& title);

    /**
     * @brief Extract multiple pages efficiently
     *
     * Groups pages by chunk for efficient extraction.
     */
    [[nodiscard]] std::vector<ExtractedPage> extract_pages(const std::vector<std::string>& titles);

    /**
     * @brief Decompress a chunk by index
     * @param chunk_idx Index of chunk (0 to chunk_count()-1)
     * @return Decompressed XML content
     */
    [[nodiscard]] std::string decompress_chunk(size_t chunk_idx);

    /**
     * @brief Decompress a chunk by offset range
     * @param start_offset Start offset in compressed file
     * @param length Length in compressed file
     * @return Decompressed content
     */
    [[nodiscard]] std::string decompress_chunk(uint64_t start_offset, uint64_t length);

    /**
     * @brief Progress info for process_all
     */
    struct ProcessProgress {
        size_t pages_processed = 0;
        uint64_t bytes_compressed = 0;   // Bytes read from compressed file
        uint64_t bytes_total = 0;        // Total compressed file size
    };

    /**
     * @brief Process all pages in dump (streaming)
     *
     * Iterates through all chunks without loading entire index.
     * Memory efficient for processing entire dump.
     *
     * @param callback Called for each page (title, content). Return false to stop.
     */
    void process_all(std::function<bool(const std::string&, const std::string&)> callback);

    /**
     * @brief Process all pages with progress reporting
     *
     * @param callback Called for each page (title, content). Return false to stop.
     * @param progress Called periodically (every 1000 pages or 2 seconds)
     */
    void process_all(
        std::function<bool(const std::string&, const std::string&)> callback,
        std::function<void(const ProcessProgress&)> progress
    );

    /**
     * @brief Get dump path info
     */
    [[nodiscard]] const DumpPath& path() const noexcept;

    /**
     * @brief Get last error message
     */
    [[nodiscard]] const std::string& error() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// XML parsing utilities
// ============================================================================

/**
 * @brief Extract specific page from XML chunk
 * @param title Page title to find
 * @param xml_chunk Decompressed XML content (multiple pages)
 * @return Page content or empty string if not found
 */
[[nodiscard]] std::string extract_page_from_xml(
    const std::string& title,
    const std::string& xml_chunk
);

/**
 * @brief Extract all pages from XML chunk
 * @param xml_chunk Decompressed XML content
 * @return Vector of (title, content) pairs
 */
[[nodiscard]] std::vector<std::pair<std::string, std::string>> extract_all_from_xml(
    const std::string& xml_chunk
);

} // namespace wikilib::dump
