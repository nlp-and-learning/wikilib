#pragma once

/**
 * @file bz2_line_reader.h
 * @brief Line reader for BZ2 compressed files
 */

#include <memory>
#include "wikilib/core/line_reader.h"
#include "wikilib/dump/bz2_stream.h"

namespace wikilib::dump {

/**
 * @brief Buffered line reader for BZ2 compressed files
 *
 * Efficiently reads lines from BZ2 files using large buffer blocks.
 * Handles lines spanning multiple compressed blocks.
 */
class Bz2LineReader : public core::BufferedLineReader {
public:
    /**
     * @brief Create reader from BZ2 file path
     * @param path Path to BZ2 file
     * @param buffer_size Buffer size for reading (default 512KB)
     */
    explicit Bz2LineReader(const std::string& path, size_t buffer_size = 512 * 1024);

    /**
     * @brief Create reader from existing Bz2Stream
     * @param stream BZ2 stream to read from (takes ownership)
     * @param buffer_size Buffer size for reading
     */
    explicit Bz2LineReader(std::unique_ptr<Bz2Stream> stream, size_t buffer_size = 512 * 1024);

    ~Bz2LineReader() override = default;

    /**
     * @brief Get last error message
     */
    [[nodiscard]] std::string_view error() const noexcept;

protected:
    void fill_buffer() override;

private:
    std::unique_ptr<Bz2Stream> stream_;
    std::string error_message_;
};

} // namespace wikilib::dump
