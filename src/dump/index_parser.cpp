/**
 * @file index_parser.cpp
 * @brief Implementation of MediaWiki dump index parser
 */

#include "wikilib/dump/index_parser.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include "wikilib/dump/index_chunker.h"

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
    void read_chunks(IndexChunker &chunker);
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

void IndexParser::Impl::read_chunks(IndexChunker &chunker) {
    IndexChunk chunk;
    while (chunker.next_chunk(chunk)) {
        for (auto &entry: chunk.entries)
            entries.push_back(std::move(entry));
    }
    if (!chunker.error().empty()) {
        error_message = chunker.error();
        entries.clear();
        return;
    }
    if (entries.empty()) {
        error_message = "No valid entries found in index file";
        return;
    }
    build_indices();
    valid = true;
}

IndexParser::IndexParser(const std::string &path) : impl_(std::make_unique<Impl>()) {
    auto chunker = IndexChunker::from_file(path, 0, IndexLinePolicy::RejectMalformed);
    impl_->read_chunks(chunker);
}

IndexParser IndexParser::from_string(std::string_view content) {
    IndexParser parser;
    std::istringstream stream{std::string(content)};
    IndexChunker chunker(std::make_unique<core::StreamLineReader>(stream), 0, IndexLinePolicy::RejectMalformed);
    parser.impl_->read_chunks(chunker);
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
    return impl_ ? std::string_view(impl_->error_message) : std::string_view{};
}

// ============================================================================
// Utility functions
// ============================================================================

std::optional<IndexEntry> parse_index_line(std::string_view line) {
    // Format: offset:page_id:title
    // Example: 659:178:tęsknota

    if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);
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
    if (offset_result.ec != std::errc{} || offset_result.ptr != offset_str.data() + offset_str.size()) {
        return std::nullopt;
    }

    // Parse page_id
    std::string_view id_str = line.substr(first_colon + 1, second_colon - first_colon - 1);
    auto id_result = std::from_chars(id_str.data(), id_str.data() + id_str.size(), entry.page_id);
    if (id_result.ec != std::errc{} || id_result.ptr != id_str.data() + id_str.size()) {
        return std::nullopt;
    }

    // Rest is the title (may contain colons)
    // Wikimedia multistream indexes retain XML escaping in titles. Decode
    // predefined entities once, so lookup names agree with decoded XML titles.
    const auto title = line.substr(second_colon + 1);
    entry.title.reserve(title.size());
    for (size_t i = 0; i < title.size();) {
        bool decoded = false;
        if (title[i] == '&') {
            for (const auto &[entity, value]: {std::pair{std::string_view("&amp;"), '&'},
                                               {"&quot;", '"'},
                                               {"&apos;", '\''},
                                               {"&lt;", '<'},
                                               {"&gt;", '>'}}) {
                if (title.substr(i).starts_with(entity)) {
                    entry.title += value;
                    i += entity.size();
                    decoded = true;
                    break;
                }
            }
        }
        if (!decoded)
            entry.title += title[i++];
    }
    if (entry.title.empty())
        return std::nullopt;

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
