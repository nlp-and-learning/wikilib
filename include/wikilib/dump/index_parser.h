#pragma once

/**
 * @file index_parser.h
 * @brief Parser for MediaWiki dump index files
 *
 * Index files have format: offset:page_id:title
 * Where offset is byte position in the compressed dump file.
 */

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "wikilib/core/types.h"

namespace wikilib::dump {

// ============================================================================
// Index entry
// ============================================================================

/**
 * @brief Single entry from the index file
 */
struct IndexEntry {
    uint64_t offset = 0; // Byte offset in compressed dump
    PageId page_id = 0; // Page ID
    std::string title; // Page title

    bool operator<(const IndexEntry &other) const {
        return offset < other.offset;
    }
};

// ============================================================================
// Index parser
// ============================================================================

/**
 * @brief Parser for dump index files
 *
 * Index files allow random access to pages in compressed dumps
 * without reading the entire dump.
 */
class IndexParser {
public:
    /**
     * @brief Parse index from file
     */
    explicit IndexParser(const std::string &path);

    /**
     * @brief Parse index from string content
     */
    static IndexParser from_string(std::string_view content);

    ~IndexParser();

    IndexParser(IndexParser &&) noexcept;
    IndexParser &operator=(IndexParser &&) noexcept;

    /**
     * @brief Check if index loaded successfully
     */
    [[nodiscard]] bool is_valid() const noexcept;

    /**
     * @brief Get number of entries
     */
    [[nodiscard]] size_t size() const noexcept;

    /**
     * @brief Get all entries
     */
    [[nodiscard]] const std::vector<IndexEntry> &entries() const;

    /**
     * @brief Find entry by page title
     */
    [[nodiscard]] const IndexEntry *find_by_title(std::string_view title) const;

    /**
     * @brief Find entry by page ID
     */
    [[nodiscard]] const IndexEntry *find_by_id(PageId id) const;

    /**
     * @brief Find all entries at a given offset
     *
     * Multiple pages can share the same offset (same BZ2 block)
     */
    [[nodiscard]] std::vector<const IndexEntry *> find_by_offset(uint64_t offset) const;

    /**
     * @brief Get unique offsets (for seeking in dump)
     */
    [[nodiscard]] std::vector<uint64_t> unique_offsets() const;

    /**
     * @brief Find offset for a page title
     */
    [[nodiscard]] std::optional<uint64_t> get_offset(std::string_view title) const;

    /**
     * @brief Get entries matching prefix
     */
    [[nodiscard]] std::vector<const IndexEntry *> find_by_prefix(std::string_view prefix) const;

    /**
     * @brief Iterate over all entries
     */
    using EntryCallback = std::function<bool(const IndexEntry &)>;
    void for_each(EntryCallback callback) const;

    /**
     * @brief Get error message if loading failed
     */
    [[nodiscard]] std::string_view error() const noexcept;
private:
    IndexParser();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Utility functions
// ============================================================================

/**
 * @brief Parse single index line
 */
[[nodiscard]] std::optional<IndexEntry> parse_index_line(std::string_view line);

/**
 * @brief Load index entries from file
 */
[[nodiscard]] std::vector<IndexEntry> load_index(const std::string &path);

/**
 * @brief Build title lookup map from entries
 */
[[nodiscard]] std::unordered_map<std::string, size_t> build_title_index(const std::vector<IndexEntry> &entries);

} // namespace wikilib::dump
