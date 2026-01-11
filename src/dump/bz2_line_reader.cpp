/**
 * @file bz2_line_reader.cpp
 * @brief Implementation of BZ2 line reader
 */

#include "wikilib/dump/bz2_line_reader.h"

namespace wikilib::dump {

Bz2LineReader::Bz2LineReader(const std::string& path, size_t buffer_size)
    : BufferedLineReader(buffer_size)
    , stream_(std::make_unique<Bz2Stream>(path)) {
    if (!stream_->is_open()) {
        error_message_ = std::string(stream_->error());
        eof_ = true;
    }
}

Bz2LineReader::Bz2LineReader(std::unique_ptr<Bz2Stream> stream, size_t buffer_size)
    : BufferedLineReader(buffer_size)
    , stream_(std::move(stream)) {
    if (!stream_ || !stream_->is_open()) {
        error_message_ = stream_ ? std::string(stream_->error()) : "Null stream";
        eof_ = true;
    }
}

void Bz2LineReader::fill_buffer() {
    if (!stream_ || !stream_->is_open()) {
        bytes_read_ = 0;
        eof_ = true;
        return;
    }

    bytes_read_ = stream_->read(buffer_, buffer_size_);

    if (stream_->eof()) {
        eof_ = true;
    }

    if (!stream_->error().empty()) {
        error_message_ = std::string(stream_->error());
        bytes_read_ = 0;
        eof_ = true;
    }
}

std::string_view Bz2LineReader::error() const noexcept {
    return error_message_;
}

} // namespace wikilib::dump
