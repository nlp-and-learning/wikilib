/**
 * @file dump_path.cpp
 * @brief Implementation of dump path management
 */

#include "wikilib/dump/dump_path.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace wikilib::dump {

// ============================================================================
// WikiProject helpers
// ============================================================================

std::string project_to_dirname(WikiProject project) {
    switch (project) {
        case WikiProject::Wikipedia:   return "wiki";
        case WikiProject::Wiktionary:  return "wiktionary";
        case WikiProject::Wikidata:    return "wikidata";
        case WikiProject::Wikibooks:   return "wikibooks";
        case WikiProject::Wikinews:    return "wikinews";
        case WikiProject::Wikiquote:   return "wikiquote";
        case WikiProject::Wikisource:  return "wikisource";
        case WikiProject::Wikiversity: return "wikiversity";
        case WikiProject::Wikivoyage:  return "wikivoyage";
        case WikiProject::Wikimedia:   return "wikimedia";
    }
    return "wiki";  // Default
}

std::optional<WikiProject> dirname_to_project(std::string_view name) {
    if (name == "wiki")        return WikiProject::Wikipedia;
    if (name == "wiktionary")  return WikiProject::Wiktionary;
    if (name == "wikidata")    return WikiProject::Wikidata;
    if (name == "wikibooks")   return WikiProject::Wikibooks;
    if (name == "wikinews")    return WikiProject::Wikinews;
    if (name == "wikiquote")   return WikiProject::Wikiquote;
    if (name == "wikisource")  return WikiProject::Wikisource;
    if (name == "wikiversity") return WikiProject::Wikiversity;
    if (name == "wikivoyage")  return WikiProject::Wikivoyage;
    if (name == "wikimedia")   return WikiProject::Wikimedia;
    return std::nullopt;
}

// ============================================================================
// DumpPath implementation
// ============================================================================

DumpPath::DumpPath(const fs::path& base_dir)
    : base_dir_(base_dir) {
    if (!fs::exists(base_dir_)) {
        throw std::runtime_error("Dump fetcher directory does not exist: " + base_dir_.string());
    }
}

DumpPath DumpPath::find_from_cwd() {
    auto found = find_dump_fetcher_dir();
    if (!found) {
        throw std::runtime_error("Could not find wikimedia-dump-fetcher directory");
    }
    return DumpPath(*found);
}

DumpPath& DumpPath::set_project(WikiProject project) {
    project_ = project;
    date_.clear();  // Reset date to re-evaluate
    return *this;
}

DumpPath& DumpPath::set_language(const std::string& lang) {
    language_ = lang;
    return *this;
}

DumpPath& DumpPath::set_date(const std::string& date) {
    date_ = date;
    return *this;
}

const fs::path& DumpPath::base_dir() const noexcept {
    return base_dir_;
}

WikiProject DumpPath::project() const noexcept {
    return project_;
}

const std::string& DumpPath::language() const noexcept {
    return language_;
}

const std::string& DumpPath::date() const {
    if (date_.empty()) {
        find_latest_date();
    }
    return date_;
}

fs::path DumpPath::project_dir() const {
    return base_dir_ / project_to_dirname(project_);
}

fs::path DumpPath::date_dir() const {
    return project_dir() / date();
}

fs::path DumpPath::dump_path() const {
    return date_dir() / make_dump_filename();
}

fs::path DumpPath::index_path() const {
    return date_dir() / make_index_filename();
}

std::vector<std::string> DumpPath::available_dates() const {
    std::vector<std::string> dates;
    auto proj_dir = project_dir();

    if (!fs::exists(proj_dir)) {
        return dates;
    }

    for (const auto& entry : fs::directory_iterator(proj_dir)) {
        if (entry.is_directory()) {
            std::string name = entry.path().filename().string();
            if (is_valid_date_dir(name)) {
                dates.push_back(name);
            }
        }
    }

    // Sort lexicographically (newest last)
    std::sort(dates.begin(), dates.end());
    return dates;
}

std::vector<std::string> DumpPath::available_languages() const {
    std::vector<std::string> langs;
    auto dir = date_dir();

    if (!fs::exists(dir)) {
        return langs;
    }

    // Pattern: {lang}{project}-{date}-pages-articles-multistream.xml.bz2
    std::string suffix;
    if (project_ == WikiProject::Wikipedia) {
        suffix = "wiki-" + date() + "-pages-articles-multistream.xml.bz2";
    } else {
        suffix = project_to_dirname(project_) + "-" + date() + "-pages-articles-multistream.xml.bz2";
    }

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;

        std::string filename = entry.path().filename().string();
        if (filename.ends_with(suffix)) {
            // Extract language prefix
            std::string lang = filename.substr(0, filename.size() - suffix.size());
            if (!lang.empty()) {
                langs.push_back(lang);
            }
        }
    }

    std::sort(langs.begin(), langs.end());
    return langs;
}

bool DumpPath::dump_exists() const {
    return fs::exists(dump_path());
}

bool DumpPath::index_exists() const {
    return fs::exists(index_path());
}

uint64_t DumpPath::dump_size() const {
    auto path = dump_path();
    if (!fs::exists(path)) {
        return 0;
    }
    return fs::file_size(path);
}

std::string DumpPath::symbolic_name() const {
    // e.g., "plwiktionary-20260101"
    std::string proj_name;
    if (project_ == WikiProject::Wikipedia) {
        proj_name = "wiki";
    } else {
        proj_name = project_to_dirname(project_);
    }
    return language_ + proj_name + "-" + date();
}

void DumpPath::find_latest_date() const {
    auto dates = const_cast<DumpPath*>(this)->available_dates();
    if (dates.empty()) {
        throw std::runtime_error("No date directories found for project: " +
                                 project_to_dirname(project_));
    }
    date_ = dates.back();  // Last = newest (lexicographically)
}

std::string DumpPath::make_dump_filename() const {
    // Pattern: {lang}{project}-{date}-pages-articles-multistream.xml.bz2
    std::string proj_name;
    if (project_ == WikiProject::Wikipedia) {
        proj_name = "wiki";
    } else {
        proj_name = project_to_dirname(project_);
    }
    return language_ + proj_name + "-" + date() + "-pages-articles-multistream.xml.bz2";
}

std::string DumpPath::make_index_filename() const {
    // Pattern: {lang}{project}-{date}-pages-articles-multistream-index.txt.bz2
    std::string proj_name;
    if (project_ == WikiProject::Wikipedia) {
        proj_name = "wiki";
    } else {
        proj_name = project_to_dirname(project_);
    }
    return language_ + proj_name + "-" + date() + "-pages-articles-multistream-index.txt.bz2";
}

bool DumpPath::is_valid_date_dir(const std::string& name) {
    // Must be 8 characters, start with "2", all digits
    if (name.size() != 8) return false;
    if (name[0] != '2') return false;

    for (char c : name) {
        if (c < '0' || c > '9') return false;
    }

    return true;
}

// ============================================================================
// Utility functions
// ============================================================================

std::optional<fs::path> find_dump_fetcher_dir(const fs::path& start) {
    fs::path current = fs::absolute(start);

    // Search up to 10 levels up
    for (int i = 0; i < 10; i++) {
        // Check if parent is "gitmy" and sibling "wikimedia-dump-fetcher" exists
        if (current.parent_path().filename() == "gitmy") {
            fs::path fetcher = current.parent_path() / "wikimedia-dump-fetcher";
            if (fs::exists(fetcher) && fs::is_directory(fetcher)) {
                return fetcher;
            }
        }

        // Also check for wikimedia-dump-fetcher directly
        fs::path direct = current / "wikimedia-dump-fetcher";
        if (fs::exists(direct) && fs::is_directory(direct)) {
            return direct;
        }

        // Move to parent
        auto parent = current.parent_path();
        if (parent == current) break;  // Reached root
        current = parent;
    }

    return std::nullopt;
}

std::vector<std::string> read_terms_file(const std::string& filename) {
    std::vector<std::string> terms;
    std::ifstream file(filename);

    if (!file) {
        return terms;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");

        if (start != std::string::npos && end != std::string::npos) {
            terms.push_back(line.substr(start, end - start + 1));
        }
    }

    return terms;
}

std::vector<std::string> read_terms_for_language(
    const fs::path& work_dir,
    const std::string& lang
) {
    fs::path terms_file = work_dir / ("terms_to_extract." + lang + ".txt");
    return read_terms_file(terms_file.string());
}

} // namespace wikilib::dump
