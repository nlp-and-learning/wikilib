#include <gtest/gtest.h>
#include <limits>
#include "test_files.h"
#include "wikilib/dump/dump_reader.h"

using namespace wikilib::dump;
using namespace wikilib::test;

namespace {

class DumpReaderTest : public ::testing::Test {
protected:
    TempDirectory dir;
    DumpPath path{dir.path};
    uint64_t second_offset = 0;
    std::string valid_index;

    void SetUp() override {
        path.set_project(WikiProject::Wikipedia).set_language("en").set_date("20260101");
        std::filesystem::create_directories(path.date_dir());
        const auto first =
                bz2("<?xml version=\"1.0\"?><mediawiki><siteinfo/>"
                    "<page><title>A</title><id>1</id><revision><text>alpha &amp; beta</text></revision></page>"
                    "<page><title>Empty</title><id>2</id><revision><text/></revision></page>");
        const auto second = bz2("<page><title>B</title><id>3</id><revision><text> \n bravo \n </text>"
                                "</revision></page></mediawiki>");
        second_offset = first.size();
        write_file(path.dump_path(), first + second);
        valid_index = "0:1:A\n0:2:Empty\n" + std::to_string(second_offset) + ":3:B\n";
        set_index(valid_index);
    }

    void set_index(std::string_view content) {
        write_file(path.index_path(), bz2(content));
    }
};

TEST_F(DumpReaderTest, ExtractsFirstAndLastChunkAndEmptyPage) {
    DumpReader reader(path);
    reader.load_index();
    ASSERT_TRUE(reader.index_loaded()) << reader.error();
    EXPECT_EQ(reader.page_count(), 3u);
    EXPECT_EQ(reader.chunk_count(), 2u);
    const auto first = reader.extract_page("A");
    EXPECT_TRUE(first.found) << reader.error();
    EXPECT_EQ(first.content, "alpha & beta");
    EXPECT_EQ(first.id, 1u);
    const auto empty = reader.extract_page("Empty");
    EXPECT_TRUE(empty.found);
    EXPECT_TRUE(empty.content.empty());
    EXPECT_EQ(empty.id, 2u);
    const auto last = reader.extract_page("B");
    EXPECT_TRUE(last.found) << reader.error();
    EXPECT_EQ(last.content, " \n bravo \n ");
    EXPECT_FALSE(reader.extract_page("Missing").found);
    EXPECT_TRUE(reader.error().empty());
}

TEST_F(DumpReaderTest, PreservesBatchOrderAndDuplicateRequests) {
    DumpReader reader(path);
    reader.load_index();
    const std::vector<std::string> titles{"B", "Empty", "A", "A", "Missing", "Empty", "B"};
    const auto results = reader.extract_pages(titles);
    ASSERT_EQ(results.size(), titles.size());
    for (size_t i = 0; i < titles.size(); ++i) {
        EXPECT_EQ(results[i].title, titles[i]);
        EXPECT_EQ(results[i].found, titles[i] != "Missing");
    }
    EXPECT_EQ(results[2].content, results[3].content);
    EXPECT_EQ(results[0].content, results[6].content);
    EXPECT_EQ(results[1].id, results[5].id);
    EXPECT_TRUE(reader.error().empty());
}

TEST_F(DumpReaderTest, SupportsPlainIndexFallback) {
    std::filesystem::remove(path.index_path());
    auto plain = path.index_path();
    plain.replace_extension();
    write_file(plain, valid_index);
    DumpReader reader(path);
    reader.load_index();
    ASSERT_TRUE(reader.index_loaded()) << reader.error();
    EXPECT_TRUE(reader.extract_page("A").found);
}

TEST_F(DumpReaderTest, RepeatedLoadingReplacesPreviousState) {
    DumpReader reader(path);
    reader.load_index();
    reader.load_index();
    ASSERT_TRUE(reader.index_loaded());
    EXPECT_EQ(reader.chunk_count(), 2u);
    EXPECT_TRUE(reader.extract_page("A").found);
    set_index(std::to_string(second_offset) + ":3:B\n");
    reader.load_index();
    ASSERT_TRUE(reader.index_loaded()) << reader.error();
    EXPECT_EQ(reader.page_count(), 1u);
    EXPECT_EQ(reader.chunk_count(), 1u);
    EXPECT_FALSE(reader.has_page("A"));
    EXPECT_TRUE(reader.extract_page("B").found);
}

TEST_F(DumpReaderTest, FailedReloadClearsStateAndCanRecover) {
    DumpReader reader(path);
    for (const auto &bad: {std::string{}, std::string("0:1:A\nbad line\n"), std::to_string(path.dump_size()) + ":1:A\n",
                           std::to_string(second_offset) + ":3:B\n0:1:A\n", std::string("0:1:A\n0:2:A\n")}) {
        set_index(valid_index);
        reader.load_index();
        ASSERT_TRUE(reader.index_loaded());
        set_index(bad);
        reader.load_index();
        EXPECT_FALSE(reader.index_loaded());
        EXPECT_EQ(reader.page_count(), 0u);
        EXPECT_EQ(reader.chunk_count(), 0u);
        EXPECT_FALSE(reader.has_page("A"));
        EXPECT_FALSE(reader.error().empty());
    }
    set_index(valid_index);
    reader.load_index();
    EXPECT_TRUE(reader.index_loaded());
    EXPECT_TRUE(reader.error().empty());
}

TEST_F(DumpReaderTest, MissingFilesDoNotLoadIndex) {
    std::filesystem::remove(path.index_path());
    DumpReader reader(path);
    reader.load_index();
    EXPECT_FALSE(reader.index_loaded());
    EXPECT_FALSE(reader.error().empty());
    set_index(valid_index);
    std::filesystem::remove(path.dump_path());
    reader.load_index();
    EXPECT_FALSE(reader.index_loaded());
    EXPECT_EQ(reader.page_count(), 0u);
}

TEST_F(DumpReaderTest, CallbackFailureDoesNotPublishPartialIndex) {
    DumpReader reader(path);
    reader.load_index([](size_t) { throw std::runtime_error("callback failed"); });
    EXPECT_FALSE(reader.index_loaded());
    EXPECT_EQ(reader.page_count(), 0u);
    EXPECT_NE(reader.error().find("callback failed"), std::string::npos);
}

TEST_F(DumpReaderTest, TruncatedIndexDoesNotLoad) {
    auto data = bz2(valid_index);
    data.pop_back();
    write_file(path.index_path(), data);
    DumpReader reader(path);
    reader.load_index();
    EXPECT_FALSE(reader.index_loaded());
    EXPECT_EQ(reader.page_count(), 0u);
    EXPECT_NE(reader.error().find("unexpected EOF"), std::string::npos);
}

TEST_F(DumpReaderTest, RejectsInvalidRangesBeforeAllocating) {
    DumpReader reader(path);
    const auto size = path.dump_size();
    for (const auto [start, length]: {std::pair<uint64_t, uint64_t>{0, 0},
                                      {size, 1},
                                      {size - 1, 2},
                                      {std::numeric_limits<uint64_t>::max(), 1},
                                      {1, std::numeric_limits<uint64_t>::max()}}) {
        EXPECT_TRUE(reader.decompress_chunk(start, length).empty());
        EXPECT_FALSE(reader.error().empty());
    }
    EXPECT_FALSE(reader.decompress_chunk(0, second_offset).empty());
    EXPECT_TRUE(reader.error().empty());
}

TEST_F(DumpReaderTest, ExtractionWithoutLoadedIndexReportsError) {
    DumpReader reader(path);
    EXPECT_FALSE(reader.extract_page("A").found);
    EXPECT_FALSE(reader.error().empty());
    EXPECT_EQ(reader.extract_pages({"A", "A"}).size(), 2u);
    EXPECT_FALSE(reader.error().empty());
}

TEST_F(DumpReaderTest, MissingPageInIndexedChunkReportsError) {
    set_index("0:99:Wrong\n" + std::to_string(second_offset) + ":3:B\n");
    DumpReader reader(path);
    reader.load_index();
    EXPECT_FALSE(reader.extract_page("Wrong").found);
    EXPECT_FALSE(reader.error().empty());
    const auto results = reader.extract_pages({"Wrong", "B"});
    EXPECT_FALSE(results[0].found);
    EXPECT_TRUE(results[1].found);
    EXPECT_FALSE(reader.error().empty());
}

TEST_F(DumpReaderTest, CorruptXmlDoesNotReturnFoundPage) {
    write_file(path.dump_path(), bz2("<page><title>A</title><revision><text>unfinished"));
    set_index("0:1:A\n");
    DumpReader reader(path);
    reader.load_index();
    ASSERT_TRUE(reader.index_loaded());
    EXPECT_FALSE(reader.extract_page("A").found);
    EXPECT_NE(reader.error().find("XML"), std::string::npos);
}

TEST_F(DumpReaderTest, EmptyPageWithoutRevisionStillExists) {
    write_file(path.dump_path(), bz2("<page><title>A</title><id>1</id></page>"));
    set_index("0:1:A\n");
    DumpReader reader(path);
    reader.load_index();
    const auto result = reader.extract_page("A");
    EXPECT_TRUE(result.found);
    EXPECT_TRUE(result.content.empty());
}

TEST_F(DumpReaderTest, ReportsCorruptCompressedChunkAndCanRecover) {
    const auto good = bz2("<page><title>A</title><revision><text>text</text></revision></page>");
    auto bad = good;
    bad[10] ^= 1;
    write_file(path.dump_path(), bad);
    set_index("0:1:A\n");
    DumpReader reader(path);
    reader.load_index();
    ASSERT_TRUE(reader.index_loaded());
    EXPECT_FALSE(reader.extract_page("A").found);
    EXPECT_FALSE(reader.error().empty());
    write_file(path.dump_path(), good);
    reader.load_index();
    EXPECT_TRUE(reader.extract_page("A").found);
    EXPECT_TRUE(reader.error().empty());
}

} // namespace
