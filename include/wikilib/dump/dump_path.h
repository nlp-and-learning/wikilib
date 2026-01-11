#pragma once

/**
 * @file dump_path.h
 * @brief Path management for Wikimedia dump files
 *
 * Handles the structure of wikimedia-dump-fetcher directories:
 *   wikimedia-dump-fetcher/
 *     ├── wiki/20260101/
 *     │   ├── enwiki-20260101-pages-articles-multistream.xml.bz2
 *     │   └── enwiki-20260101-pages-articles-multistream-index.txt.bz2
 *     ├── wiktionary/20260101/
 *     │   └── ...
 *     └── wikidata/20251220/
 *         └── ...
 */

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wikilib::dump {

/**
 * @brief Project types in Wikimedia dump fetcher
 */
enum class WikiProject {
    Wikipedia,      // wiki/
    Wiktionary,     // wiktionary/
    Wikidata,       // wikidata/
    Wikibooks,      // wikibooks/
    Wikinews,       // wikinews/
    Wikiquote,      // wikiquote/
    Wikisource,     // wikisource/
    Wikiversity,    // wikiversity/
    Wikivoyage,     // wikivoyage/
    Wikimedia       // wikimedia/
};

/**
 * @brief Convert project enum to directory name
 */
[[nodiscard]] std::string project_to_dirname(WikiProject project);

/**
 * @brief Convert directory name to project enum
 */
[[nodiscard]] std::optional<WikiProject> dirname_to_project(std::string_view name);

/**
 * @brief Manages paths to Wikimedia dump files
 *
 * Example usage:
 * @code
 *   DumpPath path("/path/to/wikimedia-dump-fetcher");
 *   path.set_project(WikiProject::Wiktionary);
 *   path.set_language("pl");
 *   // Automatically finds latest date
 *
 *   auto dump_file = path.dump_path();      // .xml.bz2 file
 *   auto index_file = path.index_path();    // -index.txt.bz2 file
 * @endcode
 */
class DumpPath {
public:
    /**
     * @brief Create DumpPath with explicit base directory
     * @param base_dir Path to wikimedia-dump-fetcher directory
     */
    explicit DumpPath(const std::filesystem::path& base_dir);

    /**
     * @brief Create DumpPath by searching for wikimedia-dump-fetcher
     *
     * Searches parent directories until finding gitmy/wikimedia-dump-fetcher
     * or throws if not found.
     */
    static DumpPath find_from_cwd();

    /**
     * @brief Set the project type
     */
    DumpPath& set_project(WikiProject project);

    /**
     * @brief Set the language code (e.g., "pl", "en", "eo")
     */
    DumpPath& set_language(const std::string& lang);

    /**
     * @brief Set specific date (YYYYMMDD format)
     *
     * If not set, latest available date is used automatically.
     */
    DumpPath& set_date(const std::string& date);

    /**
     * @brief Get the base directory
     */
    [[nodiscard]] const std::filesystem::path& base_dir() const noexcept;

    /**
     * @brief Get current project
     */
    [[nodiscard]] WikiProject project() const noexcept;

    /**
     * @brief Get current language
     */
    [[nodiscard]] const std::string& language() const noexcept;

    /**
     * @brief Get current date (finds latest if not explicitly set)
     */
    [[nodiscard]] const std::string& date() const;

    /**
     * @brief Get path to dump XML file
     */
    [[nodiscard]] std::filesystem::path dump_path() const;

    /**
     * @brief Get path to index file
     */
    [[nodiscard]] std::filesystem::path index_path() const;

    /**
     * @brief Get the project directory (e.g., base/wiktionary)
     */
    [[nodiscard]] std::filesystem::path project_dir() const;

    /**
     * @brief Get the date directory (e.g., base/wiktionary/20260101)
     */
    [[nodiscard]] std::filesystem::path date_dir() const;

    /**
     * @brief List available dates for current project
     */
    [[nodiscard]] std::vector<std::string> available_dates() const;

    /**
     * @brief List available languages for current project and date
     */
    [[nodiscard]] std::vector<std::string> available_languages() const;

    /**
     * @brief Check if dump file exists
     */
    [[nodiscard]] bool dump_exists() const;

    /**
     * @brief Check if index file exists
     */
    [[nodiscard]] bool index_exists() const;

    /**
     * @brief Get file size of dump file
     */
    [[nodiscard]] uint64_t dump_size() const;

    /**
     * @brief Get symbolic name (e.g., "plwiktionary-20260101")
     */
    [[nodiscard]] std::string symbolic_name() const;

private:
    std::filesystem::path base_dir_;
    WikiProject project_ = WikiProject::Wiktionary;
    std::string language_ = "pl";
    mutable std::string date_;  // Lazy-evaluated

    void find_latest_date() const;
    [[nodiscard]] std::string make_dump_filename() const;
    [[nodiscard]] std::string make_index_filename() const;
    [[nodiscard]] static bool is_valid_date_dir(const std::string& name);
};

// ============================================================================
// Utility functions
// ============================================================================

/**
 * @brief Find wikimedia-dump-fetcher directory by traversing parent directories
 * @param start Starting directory to search from
 * @return Path to wikimedia-dump-fetcher or nullopt if not found
 */
[[nodiscard]] std::optional<std::filesystem::path> find_dump_fetcher_dir(
    const std::filesystem::path& start = std::filesystem::current_path()
);

/**
 * @brief Read terms to extract from file
 * @param filename Path to terms file (one term per line)
 * @return Vector of terms
 */
[[nodiscard]] std::vector<std::string> read_terms_file(const std::string& filename);

/**
 * @brief Read terms for specific language from work directory
 * @param work_dir Work directory containing terms_to_extract.*.txt files
 * @param lang Language code
 * @return Vector of terms
 */
[[nodiscard]] std::vector<std::string> read_terms_for_language(
    const std::filesystem::path& work_dir,
    const std::string& lang
);

} // namespace wikilib::dump
