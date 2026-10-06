#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace wikilib::dump {

/**
 * Incrementally decompress complete BZ2 streams in [start_offset, end_offset).
 * Input is buffered in fixed-size blocks and never read beyond end_offset.
 * The range may contain concatenated streams but must start and end at stream
 * boundaries. Inspect error() after reading, including after a short read.
 */
class Bz2RangeReader {
public:
    Bz2RangeReader(const std::string &path, uint64_t start_offset, uint64_t end_offset);
    ~Bz2RangeReader();

    Bz2RangeReader(const Bz2RangeReader &) = delete;
    Bz2RangeReader &operator=(const Bz2RangeReader &) = delete;
    Bz2RangeReader(Bz2RangeReader &&) noexcept;
    Bz2RangeReader &operator=(Bz2RangeReader &&) noexcept;

    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool eof() const noexcept;
    size_t read(char *buffer, size_t size);
    [[nodiscard]] std::string_view error() const noexcept;
    /** Bytes fetched from this range, including fixed-size read-ahead. */
    [[nodiscard]] uint64_t compressed_bytes_read() const noexcept;
    [[nodiscard]] uint64_t decompressed_bytes_read() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace wikilib::dump
