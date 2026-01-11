/**
 * @file bz2_stream.cpp
 * @brief Implementation of BZ2 compressed stream reader
 */

#include "wikilib/dump/bz2_stream.h"
#include <algorithm>
#include <bzlib.h>
#include <cstring>
#include <sys/stat.h>

namespace wikilib::dump {

// ============================================================================
// Bz2Stream implementation
// ============================================================================

struct Bz2Stream::Impl {
    FILE *file = nullptr;
    bool own_file = false;
    BZFILE *bz_file = nullptr;
    int bz_error = BZ_OK;
    bool at_eof = false;
    std::string error_message;
    uint64_t compressed_bytes = 0;
    uint64_t decompressed_bytes = 0;

    // Buffer for multistream handling
    static constexpr size_t BUFFER_SIZE = 64 * 1024;
    char buffer[BUFFER_SIZE];
    size_t buffer_pos = 0;
    size_t buffer_len = 0;

    // Line reading buffer
    std::string line_buffer;

    bool open_next_stream();
    void close_bz();
};

bool Bz2Stream::Impl::open_next_stream() {
    if (bz_file) {
        BZ2_bzReadClose(&bz_error, bz_file);
        bz_file = nullptr;
    }

    if (!file || feof(file)) {
        at_eof = true;
        return false;
    }

    bz_error = BZ_OK;
    bz_file = BZ2_bzReadOpen(&bz_error, file, 0, 0, nullptr, 0);

    if (bz_error != BZ_OK || !bz_file) {
        error_message = "Failed to open BZ2 stream";
        at_eof = true;
        return false;
    }

    return true;
}

void Bz2Stream::Impl::close_bz() {
    if (bz_file) {
        BZ2_bzReadClose(&bz_error, bz_file);
        bz_file = nullptr;
    }
}

Bz2Stream::Bz2Stream(const std::string &path) : impl_(std::make_unique<Impl>()) {
    impl_->file = fopen(path.c_str(), "rb");
    if (!impl_->file) {
        impl_->error_message = "Failed to open file: " + path;
        impl_->at_eof = true;
        return;
    }
    impl_->own_file = true;

    if (!impl_->open_next_stream()) {
        return;
    }
}

Bz2Stream::Bz2Stream(FILE *file, bool own_file) : impl_(std::make_unique<Impl>()) {
    impl_->file = file;
    impl_->own_file = own_file;

    if (!impl_->file) {
        impl_->error_message = "Invalid file pointer";
        impl_->at_eof = true;
        return;
    }

    if (!impl_->open_next_stream()) {
        return;
    }
}

Bz2Stream::~Bz2Stream() {
    close();
}

Bz2Stream::Bz2Stream(Bz2Stream &&other) noexcept : impl_(std::move(other.impl_)) {
}

Bz2Stream &Bz2Stream::operator=(Bz2Stream &&other) noexcept {
    if (this != &other) {
        close();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

bool Bz2Stream::is_open() const noexcept {
    return impl_ && impl_->file && impl_->bz_file && !impl_->at_eof;
}

bool Bz2Stream::eof() const noexcept {
    return !impl_ || impl_->at_eof;
}

size_t Bz2Stream::read(char *buffer, size_t n) {
    if (!impl_ || !impl_->bz_file || impl_->at_eof) {
        return 0;
    }

    size_t total_read = 0;

    while (total_read < n && !impl_->at_eof) {
        int bytes_read = BZ2_bzRead(&impl_->bz_error, impl_->bz_file, buffer + total_read,
                                    static_cast<int>(std::min(n - total_read, static_cast<size_t>(INT_MAX))));

        if (bytes_read > 0) {
            total_read += bytes_read;
            impl_->decompressed_bytes += bytes_read;
        }

        if (impl_->bz_error == BZ_STREAM_END) {
            // Try to open next stream (multistream BZ2)
            if (!impl_->open_next_stream()) {
                impl_->at_eof = true;
            }
        } else if (impl_->bz_error != BZ_OK) {
            impl_->error_message = "BZ2 read error";
            impl_->at_eof = true;
            break;
        }
    }

    // Update compressed bytes estimate
    if (impl_->file) {
        impl_->compressed_bytes = static_cast<uint64_t>(ftell(impl_->file));
    }

    return total_read;
}

std::optional<std::string> Bz2Stream::read_line() {
    if (!impl_ || impl_->at_eof) {
        return std::nullopt;
    }

    impl_->line_buffer.clear();

    while (true) {
        // Check if we have buffered data
        while (impl_->buffer_pos < impl_->buffer_len) {
            char c = impl_->buffer[impl_->buffer_pos++];
            if (c == '\n') {
                // Remove trailing \r if present
                if (!impl_->line_buffer.empty() && impl_->line_buffer.back() == '\r') {
                    impl_->line_buffer.pop_back();
                }
                return impl_->line_buffer;
            }
            impl_->line_buffer += c;
        }

        // Refill buffer
        impl_->buffer_pos = 0;
        impl_->buffer_len = read(impl_->buffer, Impl::BUFFER_SIZE);

        if (impl_->buffer_len == 0) {
            // EOF reached
            if (!impl_->line_buffer.empty()) {
                return impl_->line_buffer;
            }
            return std::nullopt;
        }
    }
}

std::string Bz2Stream::read_all() {
    std::string result;
    char buffer[Impl::BUFFER_SIZE];

    while (!eof()) {
        size_t n = read(buffer, Impl::BUFFER_SIZE);
        if (n == 0)
            break;
        result.append(buffer, n);
    }

    return result;
}

uint64_t Bz2Stream::compressed_bytes_read() const noexcept {
    return impl_ ? impl_->compressed_bytes : 0;
}

uint64_t Bz2Stream::decompressed_bytes_read() const noexcept {
    return impl_ ? impl_->decompressed_bytes : 0;
}

bool Bz2Stream::seek_to_stream(uint64_t offset) {
    if (!impl_ || !impl_->file) {
        return false;
    }

    // Close current BZ2 stream
    impl_->close_bz();

    // Seek in file
    if (fseek(impl_->file, static_cast<long>(offset), SEEK_SET) != 0) {
        impl_->error_message = "Seek failed";
        return false;
    }

    impl_->compressed_bytes = offset;
    impl_->at_eof = false;
    impl_->buffer_pos = 0;
    impl_->buffer_len = 0;

    // Open new BZ2 stream
    return impl_->open_next_stream();
}

std::string_view Bz2Stream::error() const noexcept {
    return impl_ ? impl_->error_message : "";
}

void Bz2Stream::close() {
    if (!impl_)
        return;

    impl_->close_bz();

    if (impl_->file && impl_->own_file) {
        fclose(impl_->file);
    }
    impl_->file = nullptr;
    impl_->at_eof = true;
}

// ============================================================================
// Utility functions
// ============================================================================

Result<std::string> decompress_bz2(std::string_view compressed) {
    if (compressed.size() < 4) {
        return std::unexpected(ParseError{"Data too short to be BZ2", {}, ErrorSeverity::Error, ""});
    }

    // Estimate output size (typically 5-10x compression ratio)
    size_t output_size = compressed.size() * 10;
    std::string result;
    result.resize(output_size);

    unsigned int dest_len = static_cast<unsigned int>(output_size);

    int ret = BZ2_bzBuffToBuffDecompress(result.data(), &dest_len, const_cast<char *>(compressed.data()),
                                         static_cast<unsigned int>(compressed.size()),
                                         0, // small
                                         0 // verbosity
    );

    if (ret == BZ_OUTBUFF_FULL) {
        // Try with larger buffer
        output_size = compressed.size() * 50;
        result.resize(output_size);
        dest_len = static_cast<unsigned int>(output_size);

        ret = BZ2_bzBuffToBuffDecompress(result.data(), &dest_len, const_cast<char *>(compressed.data()),
                                         static_cast<unsigned int>(compressed.size()), 0, 0);
    }

    if (ret != BZ_OK) {
        std::string error_msg = "BZ2 decompression failed: ";
        switch (ret) {
            case BZ_CONFIG_ERROR:
                error_msg += "config error";
                break;
            case BZ_PARAM_ERROR:
                error_msg += "param error";
                break;
            case BZ_MEM_ERROR:
                error_msg += "memory error";
                break;
            case BZ_DATA_ERROR:
                error_msg += "data error";
                break;
            case BZ_DATA_ERROR_MAGIC:
                error_msg += "not BZ2 data";
                break;
            case BZ_UNEXPECTED_EOF:
                error_msg += "unexpected EOF";
                break;
            default:
                error_msg += "unknown error";
                break;
        }
        return std::unexpected(ParseError{error_msg, {}, ErrorSeverity::Error, ""});
    }

    result.resize(dest_len);
    return result;
}

Result<std::string> compress_bz2(std::string_view data, int compression_level) {
    if (compression_level < 1)
        compression_level = 1;
    if (compression_level > 9)
        compression_level = 9;

    // BZ2 worst case is ~1.01x original size + 600 bytes
    size_t output_size = data.size() + data.size() / 100 + 600;
    std::string result;
    result.resize(output_size);

    unsigned int dest_len = static_cast<unsigned int>(output_size);

    int ret = BZ2_bzBuffToBuffCompress(result.data(), &dest_len, const_cast<char *>(data.data()),
                                       static_cast<unsigned int>(data.size()), compression_level,
                                       0, // verbosity
                                       30 // workFactor (default)
    );

    if (ret != BZ_OK) {
        std::string error_msg = "BZ2 compression failed: ";
        switch (ret) {
            case BZ_CONFIG_ERROR:
                error_msg += "config error";
                break;
            case BZ_PARAM_ERROR:
                error_msg += "param error";
                break;
            case BZ_MEM_ERROR:
                error_msg += "memory error";
                break;
            case BZ_OUTBUFF_FULL:
                error_msg += "output buffer full";
                break;
            default:
                error_msg += "unknown error";
                break;
        }
        return std::unexpected(ParseError{error_msg, {}, ErrorSeverity::Error, ""});
    }

    result.resize(dest_len);
    return result;
}

bool is_bz2(std::string_view data) noexcept {
    // BZ2 magic: "BZ" followed by version ('h') and block size ('1'-'9')
    if (data.size() < 4)
        return false;
    return data[0] == 'B' && data[1] == 'Z' && data[2] == 'h' && data[3] >= '1' && data[3] <= '9';
}

uint64_t get_file_size(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return static_cast<uint64_t>(st.st_size);
    }
    return 0;
}

} // namespace wikilib::dump
