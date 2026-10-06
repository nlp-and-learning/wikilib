#include <gtest/gtest.h>
#include <sstream>
#include "test_files.h"
#include "wikilib/dump/index_chunker.h"

using namespace wikilib::dump;
using namespace wikilib::test;

namespace {

TEST(IndexIoTest, DecodesWikimediaTitleEscapingExactlyOnce) {
    const auto entry =
            parse_index_line("10:2536003:School &quot;Milenium&quot; &amp; &apos;x&apos; &lt;y&gt; &amp;quot;");
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->title, "School \"Milenium\" & 'x' <y> &quot;");
    TempDirectory dir;
    const auto path = dir.path / "index.txt.bz2";
    write_file(path, bz2("10:1:A &amp; B\n"));
    const auto chunk = find_chunk_by_title(path.string(), 20, "A & B");
    ASSERT_TRUE(chunk);
    EXPECT_EQ(chunk->entries.front().title, "A & B");
}

TEST(IndexIoTest, ReadsPlainAndCompressedFilesIncludingCrLfAndColons) {
    TempDirectory dir;
    const std::string content = "\n0:1:A\r\n0:2:Title:With:Colons\r\n10:3:B";
    for (bool compressed: {false, true}) {
        const auto path = dir.path / (compressed ? "index.txt.bz2" : "index.txt");
        write_file(path, compressed ? bz2(content) : content);
        auto chunker = IndexChunker::from_file(path.string(), 20, IndexLinePolicy::RejectMalformed);
        IndexChunk chunk;
        ASSERT_TRUE(chunker.next_chunk(chunk));
        EXPECT_EQ(chunk.start_offset, 0u);
        EXPECT_EQ(chunk.end_offset, 10u);
        ASSERT_EQ(chunk.entries.size(), 2u);
        EXPECT_EQ(chunk.entries[1].title, "Title:With:Colons");
        ASSERT_TRUE(chunker.next_chunk(chunk));
        EXPECT_EQ(chunk.start_offset, 10u);
        EXPECT_EQ(chunk.end_offset, 20u);
        EXPECT_FALSE(chunker.next_chunk(chunk));
        EXPECT_TRUE(chunker.error().empty());
        EXPECT_EQ(count_index_entries(path.string()), 3u);
        EXPECT_EQ(load_index_chunks(path.string(), 20).size(), 2u);
        EXPECT_EQ(find_chunk_by_title(path.string(), 20, "B")->start_offset, 10u);
    }
}

TEST(IndexIoTest, MovesPlainReaderWithoutLosingStreamOwnership) {
    TempDirectory dir;
    const auto path = dir.path / "index.txt";
    write_file(path, "0:1:A\n10:2:B\n");
    auto original = IndexChunker::from_file(path.string(), 20);
    auto moved = std::move(original);
    IndexChunk chunk;
    EXPECT_FALSE(original.next_chunk(chunk));
    EXPECT_TRUE(moved.next_chunk(chunk));
    EXPECT_TRUE(moved.next_chunk(chunk));
    EXPECT_FALSE(moved.next_chunk(chunk));
    EXPECT_TRUE(moved.error().empty());
}

TEST(IndexIoTest, ReportsMissingFilesAndUtilityFailures) {
    TempDirectory dir;
    for (const auto *name: {"missing.txt", "missing.txt.bz2"}) {
        const auto path = (dir.path / name).string();
        auto chunker = IndexChunker::from_file(path, 20);
        IndexChunk chunk;
        EXPECT_FALSE(chunker.next_chunk(chunk));
        EXPECT_FALSE(chunker.error().empty());
        EXPECT_THROW((void) count_index_entries(path), std::runtime_error);
        EXPECT_THROW((void) load_index_chunks(path, 20), std::runtime_error);
    }
}

TEST(IndexIoTest, RejectsDecreasingOrOutOfBoundsOffsets) {
    for (const auto *data: {"10:1:A\n0:2:B\n", "20:1:A\n", "21:1:A\n", "0:1:A\n20:2:B\n"}) {
        std::istringstream stream(data);
        IndexChunker chunker(std::make_unique<wikilib::core::StreamLineReader>(stream), 20);
        IndexChunk chunk;
        EXPECT_FALSE(chunker.next_chunk(chunk));
        EXPECT_TRUE(chunk.empty());
        EXPECT_FALSE(chunker.error().empty());
    }
}

TEST(IndexIoTest, HandlesManyMalformedLinesWithoutRecursion) {
    std::string content;
    for (int i = 0; i < 100000; ++i)
        content += "bad line\n";
    content += "0:1:A\n";
    std::istringstream stream(content);
    IndexChunker chunker(std::make_unique<wikilib::core::StreamLineReader>(stream), 20);
    IndexChunk chunk;
    ASSERT_TRUE(chunker.next_chunk(chunk));
    EXPECT_EQ(chunk.entries[0].title, "A");
    EXPECT_EQ(chunker.skipped_lines(), 100000u);
    EXPECT_TRUE(chunker.error().empty());
}

TEST(IndexIoTest, StrictModeRejectsMalformedLineAndClearsPartialChunk) {
    std::istringstream stream("0:1:A\nbad line\n10:2:B\n");
    IndexChunker chunker(std::make_unique<wikilib::core::StreamLineReader>(stream), 20,
                         IndexLinePolicy::RejectMalformed);
    IndexChunk chunk;
    EXPECT_FALSE(chunker.next_chunk(chunk));
    EXPECT_TRUE(chunk.empty());
    EXPECT_NE(chunker.error().find("line 2"), std::string_view::npos);
}

TEST(IndexIoTest, ReportsTruncatedCompressedIndex) {
    TempDirectory dir;
    auto data = bz2("0:1:A\n10:2:B\n");
    data.pop_back();
    const auto path = dir.path / "index.txt.bz2";
    write_file(path, data);
    auto chunker = IndexChunker::from_file(path.string(), 20);
    IndexChunk chunk;
    EXPECT_FALSE(chunker.next_chunk(chunk));
    EXPECT_NE(chunker.error().find("unexpected EOF"), std::string_view::npos);
}

TEST(IndexIoTest, RejectsPartialNumericFieldsAndEmptyTitles) {
    for (const auto *line: {"1x:2:A", "1:2x:A", "1:2:", "-1:2:A", "1:-2:A", "18446744073709551616:2:A"}) {
        EXPECT_FALSE(parse_index_line(line)) << line;
    }
    const auto parsed = parse_index_line("0:1:A:B\r");
    ASSERT_TRUE(parsed);
    EXPECT_EQ(parsed->title, "A:B");
}

TEST(IndexIoTest, IndexParserReadsPlainAndCompressedFiles) {
    TempDirectory dir;
    const std::string content = "0:1:A\r\n10:2:B\r\n";
    for (bool compressed: {false, true}) {
        const auto path = dir.path / (compressed ? "index.txt.bz2" : "index.txt");
        write_file(path, compressed ? bz2(content) : content);
        IndexParser parser(path.string());
        ASSERT_TRUE(parser.is_valid()) << parser.error();
        EXPECT_EQ(parser.size(), 2u);
        ASSERT_NE(parser.find_by_id(2), nullptr);
        EXPECT_EQ(parser.find_by_id(2)->title, "B");
        EXPECT_EQ(parser.get_offset("B"), 10u);
    }
}

TEST(IndexIoTest, IndexParserRejectsEmptyMalformedAndUnsortedInput) {
    for (const auto *data: {"", "0:1:A\nbad line\n", "10:1:A\n0:2:B\n"}) {
        auto parser = IndexParser::from_string(data);
        EXPECT_FALSE(parser.is_valid());
        EXPECT_EQ(parser.size(), 0u);
        EXPECT_FALSE(parser.error().empty());
    }
}

TEST(IndexIoTest, PropagatesLineReaderFailure) {
    class FailingReader : public wikilib::core::LineReader {
    public:
        bool read_line(std::string &line) override {
            if (read_)
                return false;
            line = "0:1:A";
            read_ = true;
            return true;
        }

        bool eof() const noexcept override {
            return read_;
        }

        std::string_view error() const noexcept override {
            return read_ ? "input failure" : "";
        }
    private:
        bool read_ = false;
    };

    IndexChunker chunker(std::make_unique<FailingReader>(), 20);
    IndexChunk chunk;
    EXPECT_FALSE(chunker.next_chunk(chunk));
    EXPECT_EQ(chunker.error(), "input failure");
}

TEST(IndexIoTest, PropagatesStreamIoFailure) {
    std::istringstream stream("0:1:A\n");
    stream.setstate(std::ios::badbit);
    IndexChunker chunker(std::make_unique<wikilib::core::StreamLineReader>(stream), 20);
    IndexChunk chunk;
    EXPECT_FALSE(chunker.next_chunk(chunk));
    EXPECT_NE(chunker.error().find("I/O error"), std::string_view::npos);
}

TEST(IndexIoTest, ReadsConcatenatedCompressedIndexStreams) {
    TempDirectory dir;
    const auto path = dir.path / "index.txt.bz2";
    write_file(path, bz2("0:1:A\n") + bz2("10:2:B\n"));
    auto chunker = IndexChunker::from_file(path.string(), 20);
    IndexChunk chunk;
    ASSERT_TRUE(chunker.next_chunk(chunk));
    EXPECT_EQ(chunk.entries[0].title, "A");
    ASSERT_TRUE(chunker.next_chunk(chunk));
    EXPECT_EQ(chunk.entries[0].title, "B");
    EXPECT_FALSE(chunker.next_chunk(chunk));
    EXPECT_TRUE(chunker.error().empty());
}

TEST(IndexIoTest, EmptyIndexHasCleanChunkerEofAndInvalidParserState) {
    TempDirectory dir;
    const auto path = dir.path / "index.txt";
    write_file(path, "");
    auto chunker = IndexChunker::from_file(path.string(), 20);
    IndexChunk chunk;
    EXPECT_FALSE(chunker.next_chunk(chunk));
    EXPECT_TRUE(chunker.error().empty());
    EXPECT_EQ(count_index_entries(path.string()), 0u);
    IndexParser parser(path.string());
    EXPECT_FALSE(parser.is_valid());
    EXPECT_FALSE(parser.error().empty());
}

} // namespace
