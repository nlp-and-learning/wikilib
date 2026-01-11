#pragma once

/**
 * @file bz2_stream.hpp
 * @brief BZ2 compressed stream reader for Wikipedia/Wiktionary dumps
 */

#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include "wikilib/core/types.h"

namespace wikilib::dump {

// ============================================================================
// BZ2 stream reader
// ============================================================================

/**
 * @brief Streaming BZ2 decompressor
 *
 * Efficiently reads large compressed dump files with minimal memory usage.
 * Supports multistream BZ2 files used by Wikipedia dumps.
 */
class Bz2Stream {
public:
    /**
     * @brief Open BZ2 file for reading
     */
    explicit Bz2Stream(const std::string &path);

    /**
     * @brief Open from file pointer
     */
    explicit Bz2Stream(FILE *file, bool own_file = false);

    ~Bz2Stream();

    // Non-copyable
    Bz2Stream(const Bz2Stream &) = delete;
    Bz2Stream &operator=(const Bz2Stream &) = delete;

    // Movable
    Bz2Stream(Bz2Stream &&other) noexcept;
    Bz2Stream &operator=(Bz2Stream &&other) noexcept;

    /**
     * @brief Check if stream is open and valid
     */
    [[nodiscard]] bool is_open() const noexcept;

    /**
     * @brief Check if at end of file
     */
    [[nodiscard]] bool eof() const noexcept;

    /**
     * @brief Read up to n bytes
     * @return Number of bytes read
     */
    size_t read(char *buffer, size_t n);

    /**
     * @brief Read line (up to newline or EOF)
     * @return Line content (without newline) or nullopt at EOF
     */
    [[nodiscard]] std::optional<std::string> read_line();

    /**
     * @brief Read all remaining content
     */
    [[nodiscard]] std::string read_all();

    /**
     * @brief Get compressed bytes read so far
     */
    [[nodiscard]] uint64_t compressed_bytes_read() const noexcept;

    /**
     * @brief Get decompressed bytes read so far
     */
    [[nodiscard]] uint64_t decompressed_bytes_read() const noexcept;

    /**
     * @brief Seek to offset in compressed file
     * @note Only seeks to start of BZ2 streams, not arbitrary positions
     */
    bool seek_to_stream(uint64_t offset);

    /**
     * @brief Get last error message
     */
    [[nodiscard]] std::string_view error() const noexcept;

    /**
     * @brief Close the stream
     */
    void close();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// BZ2 utilities
// ============================================================================

/**
 * @brief Decompress BZ2 data in memory
 */
[[nodiscard]] Result<std::string> decompress_bz2(std::string_view compressed);

/**
 * @brief Compress data to BZ2
 */
[[nodiscard]] Result<std::string> compress_bz2(std::string_view data, int compression_level = 9);

/**
 * @brief Check if data looks like BZ2
 */
[[nodiscard]] bool is_bz2(std::string_view data) noexcept;

/**
 * @brief Get file size (for progress tracking)
 */
[[nodiscard]] uint64_t get_file_size(const std::string &path);

} // namespace wikilib::dump
