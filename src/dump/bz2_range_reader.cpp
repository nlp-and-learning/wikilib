#include "wikilib/dump/bz2_range_reader.h"
#include <algorithm>
#include <array>
#include <bzlib.h>
#include <cstdio>
#include <limits>
#include <sys/stat.h>

namespace wikilib::dump {

namespace {

std::string range_error(int code) {
    switch (code) {
        case BZ_UNEXPECTED_EOF:
            return "BZ2 range error: unexpected EOF (truncated stream)";
        case BZ_DATA_ERROR:
            return "BZ2 range error: data error (corrupt stream)";
        case BZ_DATA_ERROR_MAGIC:
            return "BZ2 range error: not BZ2 data";
        case BZ_IO_ERROR:
            return "BZ2 range error: file I/O error";
        case BZ_MEM_ERROR:
            return "BZ2 range error: out of memory";
        default:
            return "BZ2 range error: code " + std::to_string(code);
    }
}

} // namespace

struct Bz2RangeReader::Impl {
    FILE *file = nullptr;
    bz_stream stream{};
    bool initialized = false;
    bool stream_ended = false;
    bool at_eof = false;
    uint64_t length = 0;
    uint64_t compressed_bytes = 0;
    uint64_t decompressed_bytes = 0;
    std::string error_message;
    std::array<char, 64 * 1024> input;

    ~Impl() {
        if (initialized)
            BZ2_bzDecompressEnd(&stream);
        if (file)
            std::fclose(file);
    }

    bool start_stream() {
        char *remaining = stream.next_in;
        const unsigned int count = stream.avail_in;
        if (initialized) {
            BZ2_bzDecompressEnd(&stream);
            initialized = false;
        }
        stream = {};
        const int result = BZ2_bzDecompressInit(&stream, 0, 0);
        if (result != BZ_OK) {
            error_message = range_error(result);
            at_eof = true;
            return false;
        }
        initialized = true;
        stream.next_in = remaining;
        stream.avail_in = count;
        stream_ended = false;
        return true;
    }
};

Bz2RangeReader::Bz2RangeReader(const std::string &path, uint64_t start, uint64_t end) :
    impl_(std::make_unique<Impl>()) {
    impl_->file = std::fopen(path.c_str(), "rb");
    if (!impl_->file) {
        impl_->error_message = "Failed to open BZ2 range file: " + path;
        impl_->at_eof = true;
        return;
    }
    // fread must not prefetch compressed bytes outside the requested range.
    // Our own fixed input buffer provides buffering instead.
    if (std::setvbuf(impl_->file, nullptr, _IONBF, 0) != 0) {
        impl_->error_message = range_error(BZ_IO_ERROR);
        impl_->at_eof = true;
        return;
    }
    struct stat info{};
    if (::fstat(::fileno(impl_->file), &info) != 0 || info.st_size < 0) {
        impl_->error_message = range_error(BZ_IO_ERROR);
        impl_->at_eof = true;
        return;
    }
    if (start >= end || end > static_cast<uint64_t>(info.st_size) ||
        start > static_cast<uint64_t>(std::numeric_limits<off_t>::max())) {
        impl_->error_message = "Invalid compressed range";
        impl_->at_eof = true;
        return;
    }
    if (::fseeko(impl_->file, static_cast<off_t>(start), SEEK_SET) != 0) {
        impl_->error_message = "BZ2 range seek failed";
        impl_->at_eof = true;
        return;
    }
    impl_->length = end - start;
    impl_->start_stream();
}

Bz2RangeReader::~Bz2RangeReader() = default;
Bz2RangeReader::Bz2RangeReader(Bz2RangeReader &&) noexcept = default;
Bz2RangeReader &Bz2RangeReader::operator=(Bz2RangeReader &&) noexcept = default;

bool Bz2RangeReader::is_open() const noexcept {
    return impl_ && impl_->file && impl_->initialized && !impl_->at_eof;
}

bool Bz2RangeReader::eof() const noexcept {
    return !impl_ || impl_->at_eof;
}

size_t Bz2RangeReader::read(char *buffer, size_t size) {
    if (!is_open() || size == 0)
        return 0;
    size_t total = 0;
    while (total < size && !impl_->at_eof) {
        if (impl_->stream_ended && !impl_->start_stream())
            break;
        if (impl_->stream.avail_in == 0) {
            const auto remaining = impl_->length - impl_->compressed_bytes;
            if (remaining != 0) {
                const auto wanted = static_cast<size_t>(std::min<uint64_t>(remaining, impl_->input.size()));
                const auto count = std::fread(impl_->input.data(), 1, wanted, impl_->file);
                impl_->compressed_bytes += count;
                if (count == 0) {
                    impl_->error_message = range_error(std::ferror(impl_->file) ? BZ_IO_ERROR : BZ_UNEXPECTED_EOF);
                    impl_->at_eof = true;
                    break;
                }
                impl_->stream.next_in = impl_->input.data();
                impl_->stream.avail_in = static_cast<unsigned int>(count);
            }
        }
        const auto wanted =
                static_cast<unsigned int>(std::min<size_t>(size - total, std::numeric_limits<unsigned int>::max()));
        impl_->stream.next_out = buffer + total;
        impl_->stream.avail_out = wanted;
        const auto input_before = impl_->stream.avail_in;
        const int result = BZ2_bzDecompress(&impl_->stream);
        const auto count = wanted - impl_->stream.avail_out;
        total += count;
        impl_->decompressed_bytes += count;
        if (result == BZ_STREAM_END) {
            impl_->stream_ended = true;
            if (impl_->stream.avail_in == 0 && impl_->compressed_bytes == impl_->length)
                impl_->at_eof = true;
        } else if (result != BZ_OK) {
            impl_->error_message = range_error(result);
            impl_->at_eof = true;
        } else if (count == 0 && input_before == impl_->stream.avail_in) {
            impl_->error_message =
                    input_before == 0 ? range_error(BZ_UNEXPECTED_EOF) : "BZ2 range decompressor made no progress";
            impl_->at_eof = true;
        }
    }
    return total;
}

std::string_view Bz2RangeReader::error() const noexcept {
    return impl_ ? std::string_view(impl_->error_message) : std::string_view{};
}

uint64_t Bz2RangeReader::compressed_bytes_read() const noexcept {
    return impl_ ? impl_->compressed_bytes : 0;
}

uint64_t Bz2RangeReader::decompressed_bytes_read() const noexcept {
    return impl_ ? impl_->decompressed_bytes : 0;
}

} // namespace wikilib::dump
