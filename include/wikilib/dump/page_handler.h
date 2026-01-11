#pragma once

/**
 * @file page_handler.hpp
 * @brief Page-level handling for MediaWiki dump processing
 */

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "wikilib/core/types.h"
#include "wikilib/dump/xml_reader.h"

namespace wikilib::dump {

// ============================================================================
// Page data structures
// ============================================================================

/**
 * @brief Revision data from dump
 */
struct Revision {
    RevisionId id = 0;
    RevisionId parent_id = 0;
    std::string timestamp;
    std::string contributor;
    std::string comment;
    std::string content;
    std::string model; // "wikitext", "css", etc.
    std::string format; // "text/x-wiki", etc.
    std::string sha1;

    [[nodiscard]] bool is_wikitext() const {
        return model.empty() || model == "wikitext";
    }
};

/**
 * @brief Complete page data from dump
 */
struct Page {
    PageInfo info;
    std::vector<Revision> revisions; // Usually just one

    /**
     * @brief Get latest revision
     */
    [[nodiscard]] const Revision *latest_revision() const {
        return revisions.empty() ? nullptr : &revisions.back();
    }

    /**
     * @brief Get page content (from latest revision)
     */
    [[nodiscard]] std::string_view content() const {
        auto *rev = latest_revision();
        return rev ? std::string_view(rev->content) : std::string_view{};
    }

    /**
     * @brief Check if page is in main namespace
     */
    [[nodiscard]] bool is_main_namespace() const {
        return info.namespace_id == 0;
    }

    /**
     * @brief Check if page is a template
     */
    [[nodiscard]] bool is_template() const {
        return info.namespace_id == 10; // Template namespace
    }

    /**
     * @brief Check if page is a module (Lua)
     */
    [[nodiscard]] bool is_module() const {
        return info.namespace_id == 828; // Module namespace
    }
};

// ============================================================================
// Page callback interface
// ============================================================================

/**
 * @brief Callback for page processing
 * @return true to continue, false to stop processing
 */
using PageCallback = std::function<bool(const Page &page)>;

/**
 * @brief Filter for selecting which pages to process
 */
struct PageFilter {
    std::optional<std::vector<NamespaceId>> namespaces;
    std::optional<std::string> title_prefix;
    std::optional<std::string> title_contains;
    bool include_redirects = true;
    bool only_latest_revision = true;
    std::optional<size_t> max_pages;

    [[nodiscard]] bool matches(const Page &page) const;
};

// ============================================================================
// Page handler
// ============================================================================

/**
 * @brief Reads and parses pages from MediaWiki dump
 */
class PageHandler {
public:
    /**
     * @brief Create handler for dump file
     */
    explicit PageHandler(const std::string &dump_path);

    /**
     * @brief Create handler from XML reader
     */
    explicit PageHandler(std::unique_ptr<XmlReader> reader);

    ~PageHandler();

    // Non-copyable, movable
    PageHandler(const PageHandler &) = delete;
    PageHandler &operator=(const PageHandler &) = delete;
    PageHandler(PageHandler &&) noexcept;
    PageHandler &operator=(PageHandler &&) noexcept;

    /**
     * @brief Process all pages with callback
     */
    void process(PageCallback callback);

    /**
     * @brief Process pages matching filter
     */
    void process(PageCallback callback, const PageFilter &filter);

    /**
     * @brief Get next page
     */
    [[nodiscard]] std::optional<Page> next_page();

    /**
     * @brief Get next page matching filter
     */
    [[nodiscard]] std::optional<Page> next_page(const PageFilter &filter);

    /**
     * @brief Get site info from dump header
     */
    struct SiteInfo {
        std::string site_name;
        std::string db_name;
        std::string base_url;
        std::string generator;
        std::vector<Namespace> namespaces;
    };

    [[nodiscard]] const SiteInfo &site_info() const;

    /**
     * @brief Get processing statistics
     */
    struct Stats {
        uint64_t pages_read = 0;
        uint64_t pages_processed = 0;
        uint64_t pages_skipped = 0;
        uint64_t bytes_processed = 0;
        uint64_t redirects_found = 0;
    };

    [[nodiscard]] const Stats &stats() const noexcept;

    /**
     * @brief Check if at end of dump
     */
    [[nodiscard]] bool eof() const noexcept;

    /**
     * @brief Get any error
     */
    [[nodiscard]] std::string_view error() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Convenience functions
// ============================================================================

/**
 * @brief Process dump file with callback
 */
void process_dump(const std::string &path, PageCallback callback, const PageFilter &filter = {});

/**
 * @brief Count pages in dump
 */
[[nodiscard]] size_t count_pages(const std::string &path, const PageFilter &filter = {});

/**
 * @brief Get single page by title
 */
[[nodiscard]] std::optional<Page> find_page(const std::string &path, std::string_view title);

/**
 * @brief Extract all templates from dump
 */
[[nodiscard]] std::vector<Page> extract_templates(const std::string &path);

} // namespace wikilib::dump
