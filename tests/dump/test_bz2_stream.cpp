#include <array>
#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>
#include "wikilib/dump/bz2_stream.h"

using wikilib::dump::Bz2Stream;
using wikilib::dump::compress_bz2;

namespace {

std::string compressed(std::string_view text) {
    auto result = compress_bz2(text);
    if (!result) {
        throw std::runtime_error("Failed to prepare compressed test input");
    }
    return *result;
}

class Bz2StreamTest : public ::testing::Test {
protected:
    struct CloseFile {
        void operator()(FILE *file) const {
            std::fclose(file);
        }
    };

    std::unique_ptr<FILE, CloseFile> file_;

    std::unique_ptr<Bz2Stream> open(std::string_view data) {
        file_.reset(std::tmpfile());
        if (!file_ || std::fwrite(data.data(), 1, data.size(), file_.get()) != data.size()) {
            throw std::runtime_error("Failed to prepare temporary test file");
        }
        std::rewind(file_.get());
        return std::make_unique<Bz2Stream>(file_.get());
    }
};

TEST_F(Bz2StreamTest, ReadsSingleStreamAndCleanEof) {
    const std::string text("hello\0world\n", 12);
    auto stream = open(compressed(text));
    EXPECT_EQ(stream->read_all(), text);
    EXPECT_TRUE(stream->eof());
    EXPECT_TRUE(stream->error().empty());
    EXPECT_EQ(stream->decompressed_bytes_read(), text.size());
    char byte;
    EXPECT_EQ(stream->read(&byte, 1), 0u);
}

TEST_F(Bz2StreamTest, ReadsConcatenatedStreamsWithBufferedInput) {
    auto stream = open(compressed("one\ntwo\n") + compressed("three\nfour\n"));
    EXPECT_EQ(stream->read_all(), "one\ntwo\nthree\nfour\n");
    EXPECT_TRUE(stream->eof());
    EXPECT_TRUE(stream->error().empty());
}

TEST_F(Bz2StreamTest, ReadsManyStreamsWithOneByteReads) {
    std::string data;
    std::string expected;
    for (int i = 0; i < 400; ++i) {
        const auto text = std::to_string(i) + "\n";
        data += compressed(text);
        expected += text;
    }
    auto stream = open(data);
    std::string result;
    char byte;
    while (stream->read(&byte, 1) == 1) {
        result += byte;
    }
    EXPECT_EQ(result, expected);
    EXPECT_TRUE(stream->error().empty());
}

TEST_F(Bz2StreamTest, ReadsEmptyStreamsBetweenNonemptyStreams) {
    auto stream = open(compressed("") + compressed("a") + compressed("") + compressed("b") + compressed(""));
    EXPECT_EQ(stream->read_all(), "ab");
    EXPECT_TRUE(stream->error().empty());
}

TEST_F(Bz2StreamTest, ValidEmptyStreamIsCleanEof) {
    auto stream = open(compressed(""));
    EXPECT_EQ(stream->read_all(), "");
    EXPECT_TRUE(stream->eof());
    EXPECT_TRUE(stream->error().empty());
}

TEST_F(Bz2StreamTest, ReadsAcrossCompressedAndOutputBufferBoundaries) {
    // Deterministic, poorly compressible data crosses libbz2's input buffer.
    std::string text;
    unsigned int state = 123456789;
    for (size_t i = 0; i < 160000; ++i) {
        state = state * 1664525u + 1013904223u;
        text += static_cast<char>(state >> 24);
    }
    auto stream = open(compressed(text) + compressed(text) + compressed("end"));
    std::string result;
    std::array<char, 997> buffer;
    while (auto count = stream->read(buffer.data(), buffer.size())) {
        result.append(buffer.data(), count);
    }
    EXPECT_TRUE(result == text + text + "end");
    EXPECT_EQ(stream->decompressed_bytes_read(), result.size());
    EXPECT_TRUE(stream->error().empty());
}

TEST_F(Bz2StreamTest, ReadsAllBufferedLinesAfterUnderlyingEof) {
    auto stream = open(compressed("one\ntwo\nthree\n"));
    EXPECT_EQ(stream->read_line(), "one");
    EXPECT_FALSE(stream->eof());
    EXPECT_EQ(stream->read_line(), "two");
    EXPECT_EQ(stream->read_line(), "three");
    EXPECT_TRUE(stream->eof());
    EXPECT_FALSE(stream->read_line());
    EXPECT_TRUE(stream->error().empty());
}

TEST_F(Bz2StreamTest, ReturnsFinalLineWithoutNewlineExactlyOnce) {
    auto stream = open(compressed("one\ntwo"));
    EXPECT_EQ(stream->read_line(), "one");
    EXPECT_EQ(stream->read_line(), "two");
    EXPECT_FALSE(stream->read_line());
    EXPECT_FALSE(stream->read_line());
    EXPECT_TRUE(stream->error().empty());
}

TEST_F(Bz2StreamTest, PreservesEmptyLinesAndRemovesCrBeforeNewline) {
    auto stream = open(compressed("\n\r\nx\r\n\n"));
    EXPECT_EQ(stream->read_line(), "");
    EXPECT_EQ(stream->read_line(), "");
    EXPECT_EQ(stream->read_line(), "x");
    EXPECT_EQ(stream->read_line(), "");
    EXPECT_FALSE(stream->read_line());
}

TEST_F(Bz2StreamTest, ReadsLinesAcrossOutputBuffersAndStreams) {
    const std::string long_line(65535, 'x');
    auto stream = open(compressed(long_line + "\r") + compressed("\nlast"));
    const auto line = stream->read_line();
    EXPECT_TRUE(line && *line == long_line);
    EXPECT_EQ(stream->read_line(), "last");
    EXPECT_FALSE(stream->read_line());
    EXPECT_TRUE(stream->error().empty());
}

TEST_F(Bz2StreamTest, ReadsLineLongerThanOutputBuffer) {
    const std::string long_line(2 * 65536 + 1, 'x');
    auto stream = open(compressed(long_line + "\nlast\n"));
    const auto line = stream->read_line();
    EXPECT_TRUE(line && *line == long_line);
    EXPECT_EQ(stream->read_line(), "last");
    EXPECT_FALSE(stream->read_line());
    EXPECT_TRUE(stream->error().empty());
}

TEST_F(Bz2StreamTest, ReadsLineEndingExactlyAtOutputBufferBoundary) {
    const std::string line_text(65535, 'x');
    auto stream = open(compressed(line_text + "\nlast\n"));
    const auto line = stream->read_line();
    EXPECT_TRUE(line && *line == line_text);
    EXPECT_EQ(stream->read_line(), "last");
    EXPECT_FALSE(stream->read_line());
    EXPECT_TRUE(stream->error().empty());
}

TEST_F(Bz2StreamTest, ReportsTruncatedStream) {
    auto data = compressed("some text\n");
    data.pop_back();
    auto stream = open(data);
    (void) stream->read_all();
    EXPECT_TRUE(stream->eof());
    EXPECT_NE(stream->error().find("unexpected EOF"), std::string_view::npos);
}

TEST_F(Bz2StreamTest, ReportsTruncatedSecondStream) {
    auto second = compressed("second");
    second.resize(second.size() / 2);
    auto stream = open(compressed("first") + second);
    const auto result = stream->read_all();
    EXPECT_TRUE(result.starts_with("first"));
    EXPECT_NE(stream->error().find("unexpected EOF"), std::string_view::npos);
}

TEST_F(Bz2StreamTest, ReportsCorruptStream) {
    auto data = compressed("some text");
    data[10] ^= 1; // Damage the block CRC, leaving the header intact.
    auto stream = open(data);
    (void) stream->read_all();
    EXPECT_NE(stream->error().find("data error"), std::string_view::npos);
}

TEST_F(Bz2StreamTest, ReportsTrailingGarbage) {
    auto stream = open(compressed("first") + "garbage");
    EXPECT_EQ(stream->read_all(), "first");
    EXPECT_NE(stream->error().find("not BZ2 data"), std::string_view::npos);
}

TEST_F(Bz2StreamTest, EmptyFileIsTruncatedInput) {
    auto stream = open("");
    EXPECT_EQ(stream->read_all(), "");
    EXPECT_NE(stream->error().find("unexpected EOF"), std::string_view::npos);
}

TEST_F(Bz2StreamTest, SeekDiscardsBufferedLinesAndRestartsAfterEof) {
    const auto first = compressed("one\ntwo\n");
    auto stream = open(first + compressed("three\nfour\n"));
    EXPECT_EQ(stream->read_line(), "one");
    ASSERT_TRUE(stream->seek_to_stream(first.size()));
    EXPECT_EQ(stream->read_line(), "three");
    EXPECT_EQ(stream->read_line(), "four");
    EXPECT_FALSE(stream->read_line());
    ASSERT_TRUE(stream->seek_to_stream(0));
    EXPECT_EQ(stream->read_all(), "one\ntwo\nthree\nfour\n");
    EXPECT_TRUE(stream->error().empty());
}

TEST_F(Bz2StreamTest, SeekClearsPreviousError) {
    const std::string garbage = "invalid header";
    auto stream = open(garbage + compressed("valid"));
    (void) stream->read_all();
    ASSERT_FALSE(stream->error().empty());
    ASSERT_TRUE(stream->seek_to_stream(garbage.size()));
    EXPECT_TRUE(stream->error().empty());
    EXPECT_EQ(stream->read_all(), "valid");
    EXPECT_TRUE(stream->error().empty());
}

} // namespace
