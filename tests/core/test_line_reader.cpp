/**
 * @file test_line_reader.cpp
 * @brief Tests for buffered line reader
 */

#include <gtest/gtest.h>
#include <sstream>
#include <cstring>
#include "wikilib/core/line_reader.h"

using namespace wikilib::core;

// ============================================================================
// StreamLineReader tests
// ============================================================================

TEST(LineReaderTest, StreamReader_SingleLine) {
    std::istringstream iss("Hello World");
    StreamLineReader reader(iss);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Hello World");

    EXPECT_FALSE(reader.read_line(line));
    EXPECT_TRUE(reader.eof());
}

TEST(LineReaderTest, StreamReader_MultipleLines) {
    std::istringstream iss("Line 1\nLine 2\nLine 3");
    StreamLineReader reader(iss);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 1");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 2");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 3");

    EXPECT_FALSE(reader.read_line(line));
}

TEST(LineReaderTest, StreamReader_EmptyLines) {
    std::istringstream iss("Line 1\n\nLine 3");
    StreamLineReader reader(iss);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 1");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 3");
}

TEST(LineReaderTest, StreamReader_DifferentLineEndings) {
    std::istringstream iss("Unix\nWindows\r\nOld Mac\rMixed");
    StreamLineReader reader(iss);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Unix");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Windows\r");  // \r\n: getline stops at \n, \r remains

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Old Mac\rMixed");  // \r is not handled by getline
}

// ============================================================================
// BufferedLineReader tests - using test implementation
// ============================================================================

// Test implementation of BufferedLineReader
class TestBufferedLineReader : public BufferedLineReader {
public:
    explicit TestBufferedLineReader(const std::string& content, size_t buffer_size = 64)
        : BufferedLineReader(buffer_size)
        , content_(content)
        , pos_(0) {
    }

protected:
    void fill_buffer() override {
        if (pos_ >= content_.size()) {
            bytes_read_ = 0;
            eof_ = true;
            return;
        }

        size_t to_read = std::min(buffer_size_, content_.size() - pos_);
        std::memcpy(buffer_, content_.data() + pos_, to_read);
        bytes_read_ = to_read;
        pos_ += to_read;

        if (pos_ >= content_.size()) {
            eof_ = true;
        }
    }

private:
    std::string content_;
    size_t pos_;
};

TEST(BufferedLineReaderTest, SingleLine) {
    TestBufferedLineReader reader("Hello World", 64);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Hello World");

    EXPECT_FALSE(reader.read_line(line));
}

TEST(BufferedLineReaderTest, MultipleLines) {
    TestBufferedLineReader reader("Line 1\nLine 2\nLine 3", 64);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 1");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 2");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 3");

    EXPECT_FALSE(reader.read_line(line));
}

TEST(BufferedLineReaderTest, LineSpanningBuffers) {
    // Line longer than buffer
    std::string long_line = "This is a very long line that will span multiple buffers";
    TestBufferedLineReader reader(long_line, 10);  // Small buffer

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, long_line);
}

TEST(BufferedLineReaderTest, LineSplitAcrossBuffers) {
    // Line ending exactly at buffer boundary
    TestBufferedLineReader reader("12345\n67890", 6);  // "12345\n" fits exactly

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "12345");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "67890");
}

TEST(BufferedLineReaderTest, UnixLineEndings) {
    TestBufferedLineReader reader("Line 1\nLine 2\nLine 3", 64);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 1");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 2");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 3");
}

TEST(BufferedLineReaderTest, WindowsLineEndings) {
    TestBufferedLineReader reader("Line 1\r\nLine 2\r\nLine 3", 64);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 1");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 2");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 3");
}

TEST(BufferedLineReaderTest, MacLineEndings) {
    TestBufferedLineReader reader("Line 1\rLine 2\rLine 3", 64);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 1");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 2");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 3");
}

TEST(BufferedLineReaderTest, MixedLineEndings) {
    TestBufferedLineReader reader("Unix\nWindows\r\nMac\rEnd", 64);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Unix");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Windows");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Mac");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "End");
}

TEST(BufferedLineReaderTest, EmptyLines) {
    TestBufferedLineReader reader("Line 1\n\nLine 3\n\n\nLine 6", 64);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 1");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 3");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 6");
}

TEST(BufferedLineReaderTest, EmptyInput) {
    TestBufferedLineReader reader("", 64);

    std::string line;
    EXPECT_FALSE(reader.read_line(line));
    EXPECT_TRUE(reader.eof());
}

TEST(BufferedLineReaderTest, SingleNewline) {
    TestBufferedLineReader reader("\n", 64);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "");

    EXPECT_FALSE(reader.read_line(line));
}

TEST(BufferedLineReaderTest, TrailingNewline) {
    TestBufferedLineReader reader("Line 1\nLine 2\n", 64);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 1");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 2");

    EXPECT_FALSE(reader.read_line(line));
}

TEST(BufferedLineReaderTest, NoTrailingNewline) {
    TestBufferedLineReader reader("Line 1\nLine 2", 64);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 1");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "Line 2");

    EXPECT_FALSE(reader.read_line(line));
}

TEST(BufferedLineReaderTest, VerySmallBuffer) {
    TestBufferedLineReader reader("A\nB\nC", 2);  // Tiny buffer

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "A");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "B");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "C");
}

TEST(BufferedLineReaderTest, LineEndingAtBufferBoundary) {
    // Create input where \n is exactly at buffer boundary
    TestBufferedLineReader reader("12345\n67890\nABCDE", 6);

    std::string line;
    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "12345");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "67890");

    ASSERT_TRUE(reader.read_line(line));
    EXPECT_EQ(line, "ABCDE");
}
