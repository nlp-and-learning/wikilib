/**
 * @file line_reader.cpp
 * @brief Implementation of buffered line reader
 */

#include "wikilib/core/line_reader.h"
#include <cassert>
#include <cstring>
#include <istream>

namespace wikilib::core {

// ============================================================================
// BufferedLineReader implementation
// ============================================================================

BufferedLineReader::BufferedLineReader(size_t buffer_size)
    : buffer_size_(buffer_size) {
    buffer_ = new char[buffer_size_];
}

BufferedLineReader::~BufferedLineReader() {
    delete[] buffer_;
}

BufferedLineReader::BufferedLineReader(BufferedLineReader&& other) noexcept
    : buffer_(other.buffer_)
    , buffer_size_(other.buffer_size_)
    , bytes_read_(other.bytes_read_)
    , eof_(other.eof_)
    , initialized_(other.initialized_)
    , line_start_(other.line_start_)
    , eol_pos_(other.eol_pos_)
    , result_(std::move(other.result_)) {
    other.buffer_ = nullptr;
    other.buffer_size_ = 0;
}

BufferedLineReader& BufferedLineReader::operator=(BufferedLineReader&& other) noexcept {
    if (this != &other) {
        delete[] buffer_;

        buffer_ = other.buffer_;
        buffer_size_ = other.buffer_size_;
        bytes_read_ = other.bytes_read_;
        eof_ = other.eof_;
        initialized_ = other.initialized_;
        line_start_ = other.line_start_;
        eol_pos_ = other.eol_pos_;
        result_ = std::move(other.result_);

        other.buffer_ = nullptr;
        other.buffer_size_ = 0;
    }
    return *this;
}

bool BufferedLineReader::read_line(std::string& line) {
    // Initialize on first call
    if (!initialized_) {
        fill_buffer();
        initialized_ = true;
    }

    // Check if we have data
    if (bytes_read_ == 0 && result_.empty()) {
        return false;
    }

    // Check if we're at end without any remaining data
    if (line_start_ >= bytes_read_ && eof_ && result_.empty()) {
        return false;
    }

    // Find end of line
    find_next_eol();

    // If no more data after searching but we have accumulated result, return it
    if (bytes_read_ == 0 && !result_.empty()) {
        line = result_;
        result_.clear();
        return true;
    }

    // If at end of buffer and EOF, return remaining data
    if (line_start_ >= bytes_read_ && eof_ && !result_.empty()) {
        line = result_;
        result_.clear();
        return true;
    }

    // Extract line from buffer
    if (line_start_ < bytes_read_) {
        std::string_view sv(buffer_ + line_start_, eol_pos_ - line_start_);
        result_ += sv;
    }

    // Move past EOL if not at end of buffer
    if (eol_pos_ < bytes_read_) {
        set_start_after_eol();
    } else if (eof_ && !result_.empty()) {
        // At EOF with remaining content - return it
        line_start_ = bytes_read_;
    }

    // Return the line
    line = result_;
    result_.clear();
    return !line.empty() || eol_pos_ < bytes_read_;
}

bool BufferedLineReader::eof() const noexcept {
    return eof_ && bytes_read_ == 0;
}

bool BufferedLineReader::find_eol_in_buffer() {
    eol_pos_ = bytes_read_;

    for (size_t i = line_start_; i < bytes_read_; i++) {
        if (is_eol(buffer_[i])) {
            eol_pos_ = i;
            return true;
        }
    }

    return false;
}

void BufferedLineReader::find_next_eol() {
    // Keep searching until we find EOL or reach true EOF
    while (!find_eol_in_buffer() && bytes_read_ == buffer_size_) {
        // Line extends beyond buffer - accumulate and read more
        assert(eol_pos_ == bytes_read_);
        std::string_view sv(buffer_ + line_start_, eol_pos_ - line_start_);
        result_ += sv;

        // Read next buffer
        fill_buffer();
        line_start_ = 0;
    }
}

void BufferedLineReader::set_start_after_eol() {
    assert(eol_pos_ < bytes_read_);

    line_start_ = eol_pos_;

    // Skip \n
    if (buffer_[line_start_] == '\n') {
        skip_eol();
    }
    // Skip \r and potentially \r\n
    else {
        assert(buffer_[line_start_] == '\r');
        skip_eol();

        if (line_start_ == bytes_read_) {
            return;
        }

        // Check for \n after \r
        if (buffer_[line_start_] == '\n') {
            skip_eol();
        }
    }
}

void BufferedLineReader::skip_eol() {
    line_start_++;
    assert(bytes_read_ > 0);

    // If we consumed entire buffer and not at EOF, read next buffer
    if (line_start_ == bytes_read_ && !eof_) {
        fill_buffer();
        line_start_ = 0;
    }
}

bool BufferedLineReader::is_eol(char c) noexcept {
    return c == '\n' || c == '\r';
}

// ============================================================================
// StreamLineReader implementation
// ============================================================================

StreamLineReader::StreamLineReader(std::istream& stream)
    : stream_(stream) {
}

bool StreamLineReader::read_line(std::string& line) {
    if (eof_) {
        return false;
    }

    line.clear();

    if (std::getline(stream_, line)) {
        // Successfully read a line
        if (stream_.eof()) {
            eof_ = true;
        }
        return true;
    }

    // Failed to read - we're at EOF
    eof_ = true;
    return false;
}

bool StreamLineReader::eof() const noexcept {
    return eof_;
}

} // namespace wikilib::core
