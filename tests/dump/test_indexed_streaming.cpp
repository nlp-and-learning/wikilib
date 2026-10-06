#include <fstream>
#include <gtest/gtest.h>
#include "test_files.h"
#include "wikilib/dump/dump_reader.h"

using namespace wikilib::dump;
using namespace wikilib::test;

namespace {

std::string xml_page(std::string_view title, uint64_t id, std::string_view revisions) {
    return "<page><title>" + std::string(title) + "</title><ns>0</ns><id>" + std::to_string(id) + "</id>" +
           std::string(revisions) + "</page>";
}

class IndexedStreamingTest : public ::testing::Test {
protected:
    TempDirectory dir;
    DumpPath path{dir.path};
    std::vector<IndexChunk> chunks;
    uint64_t size = 0;

    void SetUp() override {
        path.set_project(WikiProject::Wikipedia).set_language("en").set_date("20260101");
        std::filesystem::create_directories(path.date_dir());
        const auto first = bz2("<?xml version=\"1.0\"?><mediawiki><siteinfo/>" +
                               xml_page("A", 1, "<revision><text>alpha</text></revision>") +
                               xml_page("Empty", 2, "<revision><text/></revision>"));
        const auto middle = bz2(xml_page("B", 3, "<revision><text>beta</text></revision>"));
        const auto last = bz2(xml_page("C", 4,
                                       "<revision><id>10</id><text>old</text></revision>"
                                       "<revision><id>11</id><text>new</text></revision>") +
                              "</mediawiki>");
        size = first.size() + middle.size() + last.size();
        write_file(path.dump_path(), first + middle + last);
        write_file(path.index_path(), bz2("0:1:A\n0:2:Empty\n" + std::to_string(first.size()) + ":3:B\n" +
                                          std::to_string(first.size() + middle.size()) + ":4:C\n"));
        chunks = load_index_chunks(path.index_path().string(), size);
    }
};

TEST_F(IndexedStreamingTest, ReadsFirstMiddleAndLastChunksWithoutLoadingIndex) {
    DumpReader reader(path);
    std::vector<Page> pages;
    for (const auto &chunk: chunks) {
        EXPECT_TRUE(reader.process_chunk(chunk, [&](const Page &page) {
            pages.push_back(page);
            return true;
        })) << reader.error();
    }
    ASSERT_EQ(pages.size(), 4u);
    EXPECT_EQ(pages[0].info.title, "A");
    EXPECT_TRUE(pages[1].content().empty());
    EXPECT_EQ(pages[2].content(), "beta");
    EXPECT_EQ(pages[3].content(), "new");
    EXPECT_EQ(pages[3].revisions.size(), 1u);
    EXPECT_FALSE(reader.index_loaded());
    EXPECT_EQ(reader.page_count(), 0u);
}

TEST_F(IndexedStreamingTest, EscapedIndexTitlesMatchXmlAndExtractionLookup) {
    const auto data =
            bz2("<mediawiki>" +
                xml_page("School &quot;Milenium&quot; &amp; Co", 2536003, "<revision><text>content</text></revision>") +
                "</mediawiki>");
    write_file(path.dump_path(), data);
    write_file(path.index_path(), bz2("0:2536003:School &quot;Milenium&quot; &amp; Co\n"));
    const auto indexed = load_index_chunks(path.index_path().string(), data.size());
    ASSERT_EQ(indexed.size(), 1u);
    DumpReader reader(path);
    size_t seen = 0;
    ASSERT_TRUE(reader.process_chunk(indexed.front(), [&](const Page &page) {
        EXPECT_EQ(page.info.title, "School \"Milenium\" & Co");
        ++seen;
        return true;
    })) << reader.error();
    EXPECT_EQ(seen, 1u);
    reader.load_index();
    ASSERT_TRUE(reader.error().empty()) << reader.error();
    const auto page = reader.extract_page("School \"Milenium\" & Co");
    ASSERT_TRUE(page.found) << reader.error();
    EXPECT_EQ(page.content, "content");
}

TEST_F(IndexedStreamingTest, ProcessesWholeIndexWithProgress) {
    DumpReader reader(path);
    std::vector<std::string> titles;
    std::vector<DumpReader::ProcessProgress> updates;
    EXPECT_TRUE(reader.process_indexed(
            [&](const Page &page) {
                titles.push_back(page.info.title);
                return true;
            },
            [&](const auto &progress) { updates.push_back(progress); }))
            << reader.error();
    EXPECT_EQ(titles, (std::vector<std::string>{"A", "Empty", "B", "C"}));
    ASSERT_GE(updates.size(), 2u);
    EXPECT_EQ(updates.front().pages_processed, 0u);
    EXPECT_EQ(updates.back().pages_processed, 4u);
    EXPECT_EQ(updates.back().chunks_processed, 3u);
    EXPECT_EQ(updates.back().bytes_total, size);
    EXPECT_EQ(updates.back().bytes_compressed, size);
    EXPECT_FALSE(reader.index_loaded());
    EXPECT_EQ(reader.chunk_count(), 0u);
    EXPECT_TRUE(reader.error().empty());
}

TEST_F(IndexedStreamingTest, ChunkProcessingNeverReadsCorruptNeighbor) {
    std::ifstream input(path.dump_path(), std::ios::binary);
    std::string first(chunks.front().end_offset, '\0');
    input.read(first.data(), first.size());
    input.close();
    write_file(path.dump_path(), first + "corrupt neighbor");
    DumpReader reader(path);
    int count = 0;
    EXPECT_TRUE(reader.process_chunk(chunks.front(), [&](const Page &) {
        ++count;
        return true;
    })) << reader.error();
    EXPECT_EQ(count, 2);
    EXPECT_TRUE(reader.error().empty());
}

TEST_F(IndexedStreamingTest, CancellationIsDistinctFromFailure) {
    DumpReader reader(path);
    int count = 0;
    DumpReader::ProcessProgress final;
    EXPECT_FALSE(reader.process_indexed(
            [&](const Page &) {
                ++count;
                return false;
            },
            [&](const auto &progress) { final = progress; }));
    EXPECT_EQ(count, 1);
    EXPECT_EQ(final.pages_processed, 1u);
    EXPECT_EQ(final.chunks_processed, 0u);
    EXPECT_TRUE(reader.error().empty());
    count = 0;
    EXPECT_TRUE(reader.process_indexed([&](const Page &) {
        ++count;
        return true;
    }));
    EXPECT_EQ(count, 4);
}

TEST_F(IndexedStreamingTest, RejectsIncorrectIndexMetadata) {
    DumpReader reader(path);
    for (int kind = 0; kind < 4; ++kind) {
        auto chunk = chunks.front();
        if (kind == 0)
            chunk.entries[0].title = "Wrong";
        if (kind == 1)
            chunk.entries[0].page_id = 99;
        if (kind == 2)
            chunk.entries[0].offset = 99;
        if (kind == 3)
            chunk.entries.push_back(IndexEntry{0, 99, "Missing"});
        EXPECT_FALSE(reader.process_chunk(chunk, [](const Page &) { return true; }));
        EXPECT_FALSE(reader.error().empty());
    }
}

TEST_F(IndexedStreamingTest, RejectsRangeEndingInsideCompressedStream) {
    DumpReader reader(path);
    auto chunk = chunks.front();
    --chunk.end_offset;
    EXPECT_FALSE(reader.process_chunk(chunk, [](const Page &) { return true; }));
    EXPECT_NE(reader.error().find("unexpected EOF"), std::string::npos);
}

TEST_F(IndexedStreamingTest, CallbackAndProgressFailuresAreReported) {
    DumpReader reader(path);
    EXPECT_FALSE(reader.process_indexed([](const Page &) -> bool { throw std::runtime_error("callback failed"); }));
    EXPECT_NE(reader.error().find("callback failed"), std::string::npos);
    EXPECT_FALSE(reader.process_indexed([](const Page &) { return true; },
                                        [](const auto &) { throw std::runtime_error("progress failed"); }));
    EXPECT_NE(reader.error().find("progress failed"), std::string::npos);
    EXPECT_FALSE(reader.process_indexed(nullptr));
    EXPECT_FALSE(reader.error().empty());
}

TEST_F(IndexedStreamingTest, StreamingDoesNotModifyLoadedIndex) {
    DumpReader reader(path);
    reader.load_index();
    ASSERT_TRUE(reader.index_loaded());
    EXPECT_TRUE(reader.process_indexed([](const Page &) { return true; }));
    EXPECT_TRUE(reader.index_loaded());
    EXPECT_EQ(reader.page_count(), 4u);
    EXPECT_EQ(reader.chunk_count(), 3u);
}

TEST_F(IndexedStreamingTest, AcceptsPlainIndexAndReportsMalformedOrMissingIndex) {
    auto plain = path.index_path();
    plain.replace_extension();
    std::filesystem::remove(path.index_path());
    write_file(plain, "0:1:A\n0:2:Empty\n" + std::to_string(chunks[1].start_offset) + ":3:B\n" +
                              std::to_string(chunks[2].start_offset) + ":4:C\n");
    DumpReader reader(path);
    EXPECT_TRUE(reader.process_indexed([](const Page &) { return true; }));
    write_file(plain, "0:1:A\nbad line\n");
    EXPECT_FALSE(reader.process_indexed([](const Page &) { return true; }));
    EXPECT_FALSE(reader.error().empty());
    std::filesystem::remove(plain);
    EXPECT_FALSE(reader.process_indexed([](const Page &) { return true; }));
    EXPECT_FALSE(reader.error().empty());
}

TEST_F(IndexedStreamingTest, DeliversFirstPageBeforeDecompressingWholeChunk) {
    const auto first =
            bz2("<mediawiki>" + xml_page("A", 1, "<revision><text>alpha</text></revision>") + std::string(150000, ' '));
    auto corrupt_tail = bz2(xml_page("B", 2, "<revision><text>beta</text></revision>") + "</mediawiki>");
    corrupt_tail[10] ^= 1;
    write_file(path.dump_path(), first + corrupt_tail);
    IndexChunk chunk;
    chunk.end_offset = first.size() + corrupt_tail.size();
    DumpReader reader(path);
    int count = 0;
    EXPECT_FALSE(reader.process_chunk(chunk, [&](const Page &page) {
        ++count;
        EXPECT_EQ(page.info.title, "A");
        return false;
    }));
    EXPECT_EQ(count, 1);
    EXPECT_TRUE(reader.error().empty());
    EXPECT_FALSE(reader.process_chunk(chunk, [](const Page &) { return true; }));
    EXPECT_NE(reader.error().find("data error"), std::string::npos);
}

TEST_F(IndexedStreamingTest, RejectsIncompletePageXml) {
    const auto data = bz2("<page><title>A</title><id>1</id><revision><text>unfinished");
    write_file(path.dump_path(), data);
    IndexChunk chunk;
    chunk.end_offset = data.size();
    DumpReader reader(path);
    int count = 0;
    EXPECT_FALSE(reader.process_chunk(chunk, [&](const Page &) {
        ++count;
        return true;
    }));
    EXPECT_EQ(count, 0);
    EXPECT_FALSE(reader.error().empty());
}

TEST_F(IndexedStreamingTest, ReportsIntermediateProgressAndPreservesLargePageText) {
    const std::string text(200000, 'x');
    std::string xml = "<mediawiki>";
    for (int i = 1; i <= 1001; ++i)
        xml += xml_page("P", i, "<revision><text>" + (i == 1 ? text : "small") + "</text></revision>");
    xml += "</mediawiki>";
    const auto data = bz2(xml);
    write_file(path.dump_path(), data);
    IndexChunk chunk;
    chunk.end_offset = data.size();
    DumpReader reader(path);
    std::vector<DumpReader::ProcessProgress> updates;
    EXPECT_TRUE(reader.process_chunk(
            chunk,
            [&](const Page &page) {
                if (page.info.id == 1)
                    EXPECT_EQ(page.content(), text);
                return true;
            },
            [&](const auto &progress) { updates.push_back(progress); }))
            << reader.error();
    ASSERT_GE(updates.size(), 3u);
    EXPECT_EQ(updates[1].pages_processed, 1000u);
    EXPECT_EQ(updates.back().pages_processed, 1001u);
    EXPECT_EQ(updates.back().chunks_processed, 1u);
    EXPECT_EQ(updates.back().bytes_compressed, data.size());
}

TEST_F(IndexedStreamingTest, ProgressExcludesUnindexedHeader) {
    const auto header = bz2("<mediawiki><siteinfo/>");
    const auto pages = bz2(xml_page("A", 1, "<revision><text>alpha</text></revision>") + "</mediawiki>");
    write_file(path.dump_path(), header + pages);
    write_file(path.index_path(), bz2(std::to_string(header.size()) + ":1:A\n"));
    DumpReader reader(path);
    DumpReader::ProcessProgress final;
    EXPECT_TRUE(
            reader.process_indexed([](const Page &) { return true; }, [&](const auto &progress) { final = progress; }))
            << reader.error();
    EXPECT_EQ(final.pages_processed, 1u);
    EXPECT_EQ(final.bytes_total, pages.size());
    EXPECT_EQ(final.bytes_compressed, pages.size());
}

TEST_F(IndexedStreamingTest, RejectsInvalidChunkRangesAndMissingDump) {
    DumpReader reader(path);
    auto chunk = chunks.front();
    chunk.end_offset = chunk.start_offset;
    EXPECT_FALSE(reader.process_chunk(chunk, [](const Page &) { return true; }));
    EXPECT_FALSE(reader.error().empty());
    chunk.end_offset = size + 1;
    EXPECT_FALSE(reader.process_chunk(chunk, [](const Page &) { return true; }));
    EXPECT_FALSE(reader.error().empty());
    std::filesystem::remove(path.dump_path());
    EXPECT_FALSE(reader.process_indexed([](const Page &) { return true; }));
    EXPECT_FALSE(reader.error().empty());
}

} // namespace
