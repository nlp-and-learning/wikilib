/**
 * @file index_chunker.cpp
 * @brief Implementation of index chunker
 */

#include "wikilib/dump/index_chunker.h"
#include "wikilib/dump/bz2_line_reader.h"
#include "wikilib/dump/bz2_stream.h"
#include <fstream>

namespace wikilib::dump {

// ============================================================================
// IndexChunker implementation
// ============================================================================

struct IndexChunker::Impl {
    std::unique_ptr<core::LineReader> reader;
    uint64_t eof_offset = 0;
    bool at_eof = false;
    bool is_start = true;
    IndexEntry current_entry;
    size_t chunks_processed = 0;

    bool read_entry(IndexEntry& entry);
};

bool IndexChunker::Impl::read_entry(IndexEntry& entry) {
    std::string line;

    if (!reader->read_line(line)) {
        return false;
    }

    auto parsed = parse_index_line(line);
    if (!parsed) {
        // Skip invalid lines and try next
        return read_entry(entry);
    }

    entry = std::move(*parsed);
    return true;
}

IndexChunker::IndexChunker(std::unique_ptr<core::LineReader> reader, uint64_t eof_offset)
    : impl_(std::make_unique<Impl>()) {
    impl_->reader = std::move(reader);
    impl_->eof_offset = eof_offset;
}

IndexChunker IndexChunker::from_file(const std::string& index_path, uint64_t eof_offset) {
    IndexChunker chunker;
    chunker.impl_ = std::make_unique<Impl>();
    chunker.impl_->eof_offset = eof_offset;

    // Detect if file is BZ2 compressed
    if (index_path.ends_with(".bz2")) {
        // Use BZ2 line reader
        chunker.impl_->reader = std::make_unique<Bz2LineReader>(index_path);
    } else {
        // Use standard stream reader
        auto stream = std::make_unique<std::ifstream>(index_path);
        if (!stream->is_open()) {
            chunker.impl_->at_eof = true;
            return chunker;
        }
        chunker.impl_->reader = std::make_unique<core::StreamLineReader>(*stream);
    }

    return chunker;
}

IndexChunker::~IndexChunker() = default;

IndexChunker::IndexChunker(IndexChunker&&) noexcept = default;
IndexChunker& IndexChunker::operator=(IndexChunker&&) noexcept = default;

bool IndexChunker::next_chunk(IndexChunk& chunk) {
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
    chunk.end_offset = impl_->eof_offset;
    impl_->chunks_processed++;
    return true;
}

bool IndexChunker::eof() const noexcept {
    return impl_ && impl_->at_eof;
}

size_t IndexChunker::chunks_processed() const noexcept {
    return impl_ ? impl_->chunks_processed : 0;
}

// ============================================================================
// Utility functions
// ============================================================================

std::vector<IndexChunk> load_index_chunks(
    const std::string& index_path,
    uint64_t eof_offset
) {
    std::vector<IndexChunk> chunks;

    auto chunker = IndexChunker::from_file(index_path, eof_offset);

    IndexChunk chunk;
    while (chunker.next_chunk(chunk)) {
        chunks.push_back(std::move(chunk));
    }

    return chunks;
}

size_t count_index_entries(const std::string& index_path) {
    // Use arbitrary EOF offset since we're just counting
    auto chunker = IndexChunker::from_file(index_path, 0);

    size_t count = 0;
    IndexChunk chunk;
    while (chunker.next_chunk(chunk)) {
        count += chunk.size();
    }

    return count;
}

std::optional<IndexChunk> find_chunk_by_title(
    const std::string& index_path,
    uint64_t eof_offset,
    std::string_view title
) {
    auto chunker = IndexChunker::from_file(index_path, eof_offset);

    IndexChunk chunk;
    while (chunker.next_chunk(chunk)) {
        for (const auto& entry : chunk.entries) {
            if (entry.title == title) {
                return chunk;
            }
        }
    }

    return std::nullopt;
}

} // namespace wikilib::dump
