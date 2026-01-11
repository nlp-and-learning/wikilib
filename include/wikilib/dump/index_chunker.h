#pragma once

/**
 * @file index_chunker.h
 * @brief Chunking of index entries by offset for efficient dump processing
 */

#include <cstdint>
#include <memory>
#include <vector>
#include "wikilib/core/line_reader.h"
#include "wikilib/core/types.h"
#include "wikilib/dump/index_parser.h"

namespace wikilib::dump {

// ============================================================================
// Index chunk
// ============================================================================

/**
 * @brief Chunk of index entries sharing the same BZ2 offset range
 *
 * Multiple pages can be stored in the same BZ2 block, so they share
 * the same offset. A chunk groups all entries with the same starting offset.
 */
struct IndexChunk {
    uint64_t start_offset = 0;  // Start offset in compressed dump
    uint64_t end_offset = 0;    // End offset (start of next chunk or EOF)
    std::vector<IndexEntry> entries;  // All entries in this chunk

    /**
     * @brief Clear chunk data
     */
    void clear() {
        start_offset = 0;
        end_offset = 0;
        entries.clear();
    }

    /**
     * @brief Check if chunk is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return entries.empty();
    }

    /**
     * @brief Number of entries in chunk
     */
    [[nodiscard]] size_t size() const noexcept {
        return entries.size();
    }
};

// ============================================================================
// Index chunker
// ============================================================================

/**
 * @brief Groups index entries into chunks by BZ2 offset
 *
 * Reads index file and groups consecutive entries with the same offset
 * into chunks. This allows efficient random access to dump files by
 * seeking to chunk offsets.
 *
 * Example index:
 *   659:178:page1
 *   659:179:page2     <- same offset, same chunk
 *   659:180:page3     <- same offset, same chunk
 *   1024:181:page4    <- new offset, new chunk
 *
 * This creates two chunks:
 *   Chunk 1: [659, 1024) with 3 entries
 *   Chunk 2: [1024, ?) with 1 entry
 */
class IndexChunker {
public:
    /**
     * @brief Create chunker from line reader
     * @param reader Line reader for index file (takes ownership)
     * @param eof_offset Offset of end of dump file
     */
    explicit IndexChunker(std::unique_ptr<core::LineReader> reader, uint64_t eof_offset);

    /**
     * @brief Create chunker from BZ2 index file
     * @param index_path Path to index file (.txt or .txt.bz2)
     * @param eof_offset Offset of end of dump file
     */
    static IndexChunker from_file(const std::string& index_path, uint64_t eof_offset);

    ~IndexChunker();

    // Movable only
    IndexChunker(IndexChunker&&) noexcept;
    IndexChunker& operator=(IndexChunker&&) noexcept;

    /**
     * @brief Get next chunk of entries
     * @param chunk Output chunk to fill
     * @return true if chunk was read, false if EOF
     */
    bool next_chunk(IndexChunk& chunk);

    /**
     * @brief Check if at end of stream
     */
    [[nodiscard]] bool eof() const noexcept;

    /**
     * @brief Get number of chunks processed
     */
    [[nodiscard]] size_t chunks_processed() const noexcept;

private:
    IndexChunker() = default;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Utility functions
// ============================================================================

/**
 * @brief Load all chunks from index file
 * @param index_path Path to index file
 * @param eof_offset End offset of dump file
 * @return Vector of all chunks
 */
[[nodiscard]] std::vector<IndexChunk> load_index_chunks(
    const std::string& index_path,
    uint64_t eof_offset
);

/**
 * @brief Count total entries in index file
 */
[[nodiscard]] size_t count_index_entries(const std::string& index_path);

/**
 * @brief Get chunk containing a specific page title
 */
[[nodiscard]] std::optional<IndexChunk> find_chunk_by_title(
    const std::string& index_path,
    uint64_t eof_offset,
    std::string_view title
);

} // namespace wikilib::dump
