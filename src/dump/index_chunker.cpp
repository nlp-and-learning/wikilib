/**
 * @file index_chunker.cpp
 * @brief Implementation of index chunker
 */

#include "wikilib/dump/index_chunker.h"
#include <fstream>
#include <stdexcept>
#include "wikilib/dump/bz2_line_reader.h"
#include "wikilib/dump/bz2_stream.h"

namespace wikilib::dump {

// ============================================================================
// IndexChunker implementation
// ============================================================================

struct IndexChunker::Impl {
    // Destroy the borrowed reader before its underlying stream.
    std::unique_ptr<std::ifstream> plain_stream;
    std::unique_ptr<core::LineReader> reader;
    uint64_t eof_offset = 0;
    bool at_eof = false;
    bool is_start = true;
    IndexEntry current_entry;
    size_t chunks_processed = 0;
    size_t lines_read = 0;
    size_t skipped_lines = 0;
    IndexLinePolicy policy = IndexLinePolicy::SkipMalformed;
    std::string error_message;
    std::optional<uint64_t> previous_offset;

    bool read_entry(IndexEntry &entry);
};

bool IndexChunker::Impl::read_entry(IndexEntry &entry) {
    std::string line;
    while (true) {
        const bool read = reader->read_line(line);
        if (!reader->error().empty()) {
            error_message = std::string(reader->error());
            at_eof = true;
            return false;
        }
        if (!read)
            return false;
        ++lines_read;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        auto parsed = parse_index_line(line);
        if (!parsed) {
            if (policy == IndexLinePolicy::RejectMalformed) {
                error_message = "Malformed index line " + std::to_string(lines_read);
                at_eof = true;
                return false;
            }
            ++skipped_lines;
            continue;
        }
        if ((previous_offset && parsed->offset < *previous_offset) ||
            (eof_offset != 0 && parsed->offset >= eof_offset)) {
            error_message = "Invalid index offset at line " + std::to_string(lines_read);
            at_eof = true;
            return false;
        }
        previous_offset = parsed->offset;
        entry = std::move(*parsed);
        return true;
    }
}

IndexChunker::IndexChunker(std::unique_ptr<core::LineReader> reader, uint64_t eof_offset, IndexLinePolicy policy) :
    impl_(std::make_unique<Impl>()) {
    impl_->reader = std::move(reader);
    impl_->eof_offset = eof_offset;
    impl_->policy = policy;
    if (!impl_->reader) {
        impl_->error_message = "Null index line reader";
        impl_->at_eof = true;
    }
}

IndexChunker IndexChunker::from_file(const std::string &index_path, uint64_t eof_offset, IndexLinePolicy policy) {
    IndexChunker chunker;
    chunker.impl_ = std::make_unique<Impl>();
    chunker.impl_->eof_offset = eof_offset;
    chunker.impl_->policy = policy;

    // Detect if file is BZ2 compressed
    if (index_path.ends_with(".bz2")) {
        // Use BZ2 line reader
        chunker.impl_->reader = std::make_unique<Bz2LineReader>(index_path);
        if (!chunker.impl_->reader->error().empty()) {
            chunker.impl_->error_message = std::string(chunker.impl_->reader->error());
            chunker.impl_->at_eof = true;
        }
    } else {
        // Use standard stream reader
        chunker.impl_->plain_stream = std::make_unique<std::ifstream>(index_path);
        if (!chunker.impl_->plain_stream->is_open()) {
            chunker.impl_->error_message = "Failed to open index file: " + index_path;
            chunker.impl_->at_eof = true;
            return chunker;
        }
        chunker.impl_->reader = std::make_unique<core::StreamLineReader>(*chunker.impl_->plain_stream);
    }

    return chunker;
}

IndexChunker::~IndexChunker() = default;

IndexChunker::IndexChunker(IndexChunker &&) noexcept = default;
IndexChunker &IndexChunker::operator=(IndexChunker &&) noexcept = default;

bool IndexChunker::next_chunk(IndexChunk &chunk) {
    if (!impl_) {
        return false;
    }

    chunk.clear();

    if (impl_->at_eof) {
        return false;
    }

    // Read first entry if this is the start
    if (impl_->is_start) {
        if (!impl_->read_entry(impl_->current_entry)) {
            impl_->at_eof = true;
            return false;
        }
        impl_->is_start = false;
    }

    // Start new chunk with current entry
    chunk.start_offset = impl_->current_entry.offset;
    chunk.entries.push_back(impl_->current_entry);

    // Read entries until we find a different offset
    IndexEntry next_entry;
    while (impl_->read_entry(next_entry)) {
        if (next_entry.offset == impl_->current_entry.offset) {
            // Same offset - add to current chunk
            chunk.entries.push_back(next_entry);
        } else {
            // Different offset - this starts next chunk
            impl_->current_entry = next_entry;
            chunk.end_offset = next_entry.offset;
            impl_->chunks_processed++;
            return true;
        }
    }

    // Reached EOF - last chunk goes to end of file
    impl_->at_eof = true;
    if (!impl_->error_message.empty()) {
        chunk.clear();
        return false;
    }
    chunk.end_offset = impl_->eof_offset;
    impl_->chunks_processed++;
    return true;
}

bool IndexChunker::eof() const noexcept {
    return !impl_ || impl_->at_eof;
}

size_t IndexChunker::chunks_processed() const noexcept {
    return impl_ ? impl_->chunks_processed : 0;
}

std::string_view IndexChunker::error() const noexcept {
    return impl_ ? std::string_view(impl_->error_message) : std::string_view{};
}

size_t IndexChunker::skipped_lines() const noexcept {
    return impl_ ? impl_->skipped_lines : 0;
}

// ============================================================================
// Utility functions
// ============================================================================

std::vector<IndexChunk> load_index_chunks(const std::string &index_path, uint64_t eof_offset) {
    std::vector<IndexChunk> chunks;

    auto chunker = IndexChunker::from_file(index_path, eof_offset);

    IndexChunk chunk;
    while (chunker.next_chunk(chunk)) {
        chunks.push_back(std::move(chunk));
    }
    if (!chunker.error().empty())
        throw std::runtime_error(std::string(chunker.error()));

    return chunks;
}

size_t count_index_entries(const std::string &index_path) {
    // Use arbitrary EOF offset since we're just counting
    auto chunker = IndexChunker::from_file(index_path, 0);

    size_t count = 0;
    IndexChunk chunk;
    while (chunker.next_chunk(chunk)) {
        count += chunk.size();
    }
    if (!chunker.error().empty())
        throw std::runtime_error(std::string(chunker.error()));

    return count;
}

std::optional<IndexChunk> find_chunk_by_title(const std::string &index_path, uint64_t eof_offset,
                                              std::string_view title) {
    auto chunker = IndexChunker::from_file(index_path, eof_offset);

    IndexChunk chunk;
    while (chunker.next_chunk(chunk)) {
        for (const auto &entry: chunk.entries) {
            if (entry.title == title) {
                return chunk;
            }
        }
    }
    if (!chunker.error().empty())
        throw std::runtime_error(std::string(chunker.error()));
    return std::nullopt;
}

} // namespace wikilib::dump
