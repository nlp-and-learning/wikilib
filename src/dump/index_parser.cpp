/**
 * @file index_parser.cpp
 * @brief Implementation of MediaWiki dump index parser
 */

#include "wikilib/dump/index_parser.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>

namespace wikilib::dump {

// ============================================================================
// IndexParser implementation
// ============================================================================

struct IndexParser::Impl {
    std::vector<IndexEntry> entries;
    std::unordered_map<std::string, size_t> title_index;
    std::unordered_map<PageId, size_t> id_index;
    std::string error_message;
    bool valid = false;

    void build_indices();
};

void IndexParser::Impl::build_indices() {
    title_index.clear();
    id_index.clear();

    title_index.reserve(entries.size());
    id_index.reserve(entries.size());

    for (size_t i = 0; i < entries.size(); ++i) {
        title_index[entries[i].title] = i;
        id_index[entries[i].page_id] = i;
    }
}

IndexParser::IndexParser() : impl_(std::make_unique<Impl>()) {
}

IndexParser::IndexParser(const std::string &path) : impl_(std::make_unique<Impl>()) {
    std::ifstream file(path);
    if (!file) {
        impl_->error_message = "Failed to open index file: " + path;
        return;
    }

    std::string line;
    size_t line_num = 0;

    while (std::getline(file, line)) {
        ++line_num;

        if (line.empty()) {
            continue;
        }

        auto entry = parse_index_line(line);
        if (entry) {
            impl_->entries.push_back(std::move(*entry));
        } else {
            // Skip malformed lines but continue parsing
        }
    }

    if (impl_->entries.empty()) {
        impl_->error_message = "No valid entries found in index file";
        return;
    }

    impl_->build_indices();
    impl_->valid = true;
}

IndexParser IndexParser::from_string(std::string_view content) {
    IndexParser parser;

    size_t pos = 0;
    while (pos < content.size()) {
        size_t end = content.find('\n', pos);
        if (end == std::string_view::npos) {
            end = content.size();
        }

        std::string_view line = content.substr(pos, end - pos);

        // Remove \r if present
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        if (!line.empty()) {
            auto entry = parse_index_line(line);
            if (entry) {
                parser.impl_->entries.push_back(std::move(*entry));
            }
        }

        pos = end + 1;
    }

    if (!parser.impl_->entries.empty()) {
        parser.impl_->build_indices();
        parser.impl_->valid = true;
    }

    return parser;
}

IndexParser::~IndexParser() = default;

IndexParser::IndexParser(IndexParser &&) noexcept = default;
IndexParser &IndexParser::operator=(IndexParser &&) noexcept = default;

bool IndexParser::is_valid() const noexcept {
    return impl_ && impl_->valid;
}

size_t IndexParser::size() const noexcept {
    return impl_ ? impl_->entries.size() : 0;
}

const std::vector<IndexEntry> &IndexParser::entries() const {
    static const std::vector<IndexEntry> empty;
    return impl_ ? impl_->entries : empty;
}

const IndexEntry *IndexParser::find_by_title(std::string_view title) const {
    if (!impl_)
        return nullptr;

    auto it = impl_->title_index.find(std::string(title));
    if (it != impl_->title_index.end()) {
        return &impl_->entries[it->second];
    }

    return nullptr;
}

const IndexEntry *IndexParser::find_by_id(PageId id) const {
    if (!impl_)
        return nullptr;

    auto it = impl_->id_index.find(id);
    if (it != impl_->id_index.end()) {
        return &impl_->entries[it->second];
    }

    return nullptr;
}

std::vector<const IndexEntry *> IndexParser::find_by_offset(uint64_t offset) const {
    std::vector<const IndexEntry *> result;

    if (!impl_)
        return result;

    for (const auto &entry: impl_->entries) {
        if (entry.offset == offset) {
            result.push_back(&entry);
        }
    }

    return result;
}

std::vector<uint64_t> IndexParser::unique_offsets() const {
    std::vector<uint64_t> offsets;

    if (!impl_)
        return offsets;

    for (const auto &entry: impl_->entries) {
        if (offsets.empty() || offsets.back() != entry.offset) {
            offsets.push_back(entry.offset);
        }
    }

    // Sort and remove duplicates
    std::sort(offsets.begin(), offsets.end());
    offsets.erase(std::unique(offsets.begin(), offsets.end()), offsets.end());

    return offsets;
}

std::optional<uint64_t> IndexParser::get_offset(std::string_view title) const {
    const auto *entry = find_by_title(title);
    if (entry) {
        return entry->offset;
    }
    return std::nullopt;
}

std::vector<const IndexEntry *> IndexParser::find_by_prefix(std::string_view prefix) const {
    std::vector<const IndexEntry *> result;

    if (!impl_)
        return result;

    for (const auto &entry: impl_->entries) {
        if (entry.title.size() >= prefix.size() && std::string_view(entry.title).substr(0, prefix.size()) == prefix) {
            result.push_back(&entry);
        }
    }

    return result;
}

void IndexParser::for_each(EntryCallback callback) const {
    if (!impl_)
        return;

    for (const auto &entry: impl_->entries) {
        if (!callback(entry)) {
            break;
        }
    }
}

std::string_view IndexParser::error() const noexcept {
    return impl_ ? impl_->error_message : "";
}

// ============================================================================
// Utility functions
// ============================================================================

std::optional<IndexEntry> parse_index_line(std::string_view line) {
    // Format: offset:page_id:title
    // Example: 659:178:tęsknota

    // Find first colon (after offset)
    size_t first_colon = line.find(':');
    if (first_colon == std::string_view::npos || first_colon == 0) {
        return std::nullopt;
    }

    // Find second colon (after page_id)
    size_t second_colon = line.find(':', first_colon + 1);
    if (second_colon == std::string_view::npos || second_colon == first_colon + 1) {
        return std::nullopt;
    }

    IndexEntry entry;

    // Parse offset
    std::string_view offset_str = line.substr(0, first_colon);
    auto offset_result = std::from_chars(offset_str.data(), offset_str.data() + offset_str.size(), entry.offset);
    if (offset_result.ec != std::errc{}) {
        return std::nullopt;
    }

    // Parse page_id
    std::string_view id_str = line.substr(first_colon + 1, second_colon - first_colon - 1);
    auto id_result = std::from_chars(id_str.data(), id_str.data() + id_str.size(), entry.page_id);
    if (id_result.ec != std::errc{}) {
        return std::nullopt;
    }

    // Rest is the title (may contain colons)
    entry.title = std::string(line.substr(second_colon + 1));

    return entry;
}

std::vector<IndexEntry> load_index(const std::string &path) {
    IndexParser parser(path);
    if (parser.is_valid()) {
        return parser.entries();
    }
    return {};
}

std::unordered_map<std::string, size_t> build_title_index(const std::vector<IndexEntry> &entries) {
    std::unordered_map<std::string, size_t> index;
    index.reserve(entries.size());

    for (size_t i = 0; i < entries.size(); ++i) {
        index[entries[i].title] = i;
    }

    return index;
}

} // namespace wikilib::dump
