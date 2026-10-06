#include <array>
#include <gtest/gtest.h>
#include <limits>
#include "test_files.h"
#include "wikilib/dump/bz2_range_reader.h"

using namespace wikilib::dump;
using namespace wikilib::test;

namespace {

std::string read_all(Bz2RangeReader &reader) {
    std::string result;
    std::array<char, 997> buffer;
    while (auto count = reader.read(buffer.data(), buffer.size()))
        result.append(buffer.data(), count);
    return result;
}

TEST(Bz2RangeReaderTest, ReadsOnlySelectedRange) {
    TempDirectory dir;
    const auto file = dir.path / "dump.bz2";
    const auto data = bz2("selected\n");
    write_file(file, "prefix" + data + "not a BZ2 stream");
    Bz2RangeReader reader(file.string(), 6, 6 + data.size());
    EXPECT_EQ(read_all(reader), "selected\n");
    EXPECT_TRUE(reader.eof());
    EXPECT_TRUE(reader.error().empty());
    EXPECT_EQ(reader.compressed_bytes_read(), data.size());
    EXPECT_EQ(reader.decompressed_bytes_read(), 9u);
}

TEST(Bz2RangeReaderTest, ReadsConcatenatedAndEmptyStreams) {
    TempDirectory dir;
    const auto file = dir.path / "dump.bz2";
    const auto data = bz2("") + bz2("first") + bz2("") + bz2("last") + bz2("");
    write_file(file, data);
    Bz2RangeReader reader(file.string(), 0, data.size());
    EXPECT_EQ(read_all(reader), "firstlast");
    EXPECT_TRUE(reader.error().empty());
}

TEST(Bz2RangeReaderTest, ReadsAcrossInputAndOutputBuffers) {
    TempDirectory dir;
    const auto file = dir.path / "dump.bz2";
    std::string text;
    unsigned int state = 123456789;
    for (int i = 0; i < 180000; ++i) {
        state = state * 1664525u + 1013904223u;
        text += static_cast<char>(state >> 24);
    }
    const auto data = bz2(text) + bz2(std::string(200000, 'x'));
    write_file(file, data);
    Bz2RangeReader reader(file.string(), 0, data.size());
    EXPECT_TRUE(read_all(reader) == text + std::string(200000, 'x'));
    EXPECT_TRUE(reader.error().empty());
    EXPECT_EQ(reader.compressed_bytes_read(), data.size());
}

TEST(Bz2RangeReaderTest, HandlesOneByteReadsAndZeroLengthRead) {
    TempDirectory dir;
    const auto file = dir.path / "dump.bz2";
    const auto data = bz2("first") + bz2("last");
    write_file(file, data);
    Bz2RangeReader reader(file.string(), 0, data.size());
    EXPECT_EQ(reader.read(nullptr, 0), 0u);
    EXPECT_EQ(reader.compressed_bytes_read(), 0u);
    std::string result;
    char byte;
    while (reader.read(&byte, 1))
        result += byte;
    EXPECT_EQ(result, "firstlast");
    EXPECT_TRUE(reader.error().empty());
}

TEST(Bz2RangeReaderTest, ReportsTruncatedEndWithoutReadingBeyondIt) {
    TempDirectory dir;
    const auto file = dir.path / "dump.bz2";
    const auto data = bz2("first");
    write_file(file, data + bz2("next"));
    Bz2RangeReader reader(file.string(), 0, data.size() - 1);
    (void) read_all(reader);
    EXPECT_NE(reader.error().find("unexpected EOF"), std::string_view::npos);
    EXPECT_EQ(reader.compressed_bytes_read(), data.size() - 1);
}

TEST(Bz2RangeReaderTest, ReportsInvalidStartAndCorruptData) {
    TempDirectory dir;
    const auto file = dir.path / "dump.bz2";
    auto data = bz2("first");
    data[10] ^= 1;
    write_file(file, data);
    Bz2RangeReader corrupt(file.string(), 0, data.size());
    (void) read_all(corrupt);
    EXPECT_NE(corrupt.error().find("data error"), std::string_view::npos);
    Bz2RangeReader invalid_start(file.string(), 1, data.size());
    (void) read_all(invalid_start);
    EXPECT_NE(invalid_start.error().find("not BZ2 data"), std::string_view::npos);
}

TEST(Bz2RangeReaderTest, RejectsInvalidAndOverflowingRanges) {
    TempDirectory dir;
    const auto file = dir.path / "dump.bz2";
    write_file(file, bz2("first"));
    for (const auto [start, end]:
         {std::pair<uint64_t, uint64_t>{0, 0}, {2, 1}, {0, std::numeric_limits<uint64_t>::max()}}) {
        Bz2RangeReader reader(file.string(), start, end);
        EXPECT_FALSE(reader.is_open());
        EXPECT_FALSE(reader.error().empty());
        EXPECT_EQ(reader.compressed_bytes_read(), 0u);
    }
}

TEST(Bz2RangeReaderTest, DetectsFileTruncationAfterOpening) {
    TempDirectory dir;
    const auto file = dir.path / "dump.bz2";
    const auto data = bz2("first");
    write_file(file, data);
    Bz2RangeReader reader(file.string(), 0, data.size());
    write_file(file, "");
    (void) read_all(reader);
    EXPECT_NE(reader.error().find("unexpected EOF"), std::string_view::npos);
}

TEST(Bz2RangeReaderTest, MoveTransfersOwnershipAndMissingFileReportsError) {
    TempDirectory dir;
    const auto file = dir.path / "dump.bz2";
    const auto data = bz2("first");
    write_file(file, data);
    Bz2RangeReader source(file.string(), 0, data.size());
    auto moved = std::move(source);
    EXPECT_FALSE(source.is_open());
    EXPECT_EQ(read_all(moved), "first");
    Bz2RangeReader missing((dir.path / "missing").string(), 0, 10);
    EXPECT_FALSE(missing.is_open());
    EXPECT_FALSE(missing.error().empty());
}

} // namespace
