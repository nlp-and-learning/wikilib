#include <gtest/gtest.h>
#include "test_files.h"
#include "wikilib/dump/dump_reader.h"

using namespace wikilib::dump;
using namespace wikilib::test;

namespace {
class ProcessingPathsTest : public ::testing::Test {
protected:
    TempDirectory dir;
    DumpPath path{dir.path};
    std::string first, last;

    void SetUp() override {
        path.set_project(WikiProject::Wikipedia).set_language("en").set_date("20260101");
        std::filesystem::create_directories(path.date_dir());
        first = bz2("<mediawiki><siteinfo/>"
                    "<page><title>A &amp; B</title><ns>0</ns><id>1</id>"
                    "<revision><text>old</text></revision>"
                    "<revision><text> \n &amp; &#65; <![CDATA[<page>literal</page>]]> \n </text></revision></page>"
                    "<page><title>Empty</title><id>2</id><revision><text/></revision></page>");
        last = bz2("<page><title>Last</title><id>3</id><revision><text>tail</text></revision></page></mediawiki>");
        write_file(path.dump_path(), first + last);
        write_file(path.index_path(), bz2("0:1:A & B\n0:2:Empty\n" + std::to_string(first.size()) + ":3:Last\n"));
    }
};

TEST_F(ProcessingPathsTest, SequentialIndexedAndExtractionAgree) {
    DumpReader reader(path);
    std::vector<std::pair<std::string, std::string>> sequential, indexed;
    reader.process_all([&](const auto &title, const auto &content) {
        sequential.emplace_back(title, content);
        return true;
    });
    ASSERT_TRUE(reader.error().empty()) << reader.error();
    ASSERT_EQ(sequential.size(), 3u);
    EXPECT_EQ(sequential[0].second, " \n & A <page>literal</page> \n ");
    ASSERT_TRUE(reader.process_indexed([&](const Page &page) {
        indexed.emplace_back(page.info.title, page.content());
        return true;
    })) << reader.error();
    EXPECT_EQ(sequential, indexed);
    reader.load_index();
    const auto results = reader.extract_pages({"Last", "A & B", "Empty", "A & B", "Missing"});
    ASSERT_EQ(results.size(), 5u);
    EXPECT_TRUE(reader.error().empty()) << reader.error();
    EXPECT_EQ(results[0].content, sequential[2].second);
    EXPECT_EQ(results[1].content, sequential[0].second);
    EXPECT_EQ(results[1].content, results[3].content);
    EXPECT_TRUE(results[2].found);
    EXPECT_TRUE(results[2].content.empty());
    EXPECT_FALSE(results[4].found);
    EXPECT_EQ(reader.extract_page("A & B").content, sequential[0].second);
}

TEST_F(ProcessingPathsTest, SequentialDoesNotRequireIndexAndResetsPreviousError) {
    DumpReader reader(path);
    reader.load_index();
    EXPECT_FALSE(reader.extract_page("Missing").found);
    std::filesystem::remove(path.index_path());
    reader.load_index();
    ASSERT_FALSE(reader.error().empty());
    int count = 0;
    reader.process_all([&](const auto &, const auto &) {
        ++count;
        return true;
    });
    EXPECT_EQ(count, 3);
    EXPECT_TRUE(reader.error().empty()) << reader.error();
}

TEST_F(ProcessingPathsTest, SequentialCancellationReportsDeliveredPages) {
    DumpReader reader(path);
    DumpReader::ProcessProgress final;
    int count = 0;
    reader.process_all(
            [&](const auto &, const auto &) {
                ++count;
                return false;
            },
            [&](const auto &progress) { final = progress; });
    EXPECT_EQ(count, 1);
    EXPECT_EQ(final.pages_processed, 1u);
    EXPECT_EQ(final.chunks_processed, 0u);
    EXPECT_EQ(final.bytes_total, path.dump_size());
    EXPECT_TRUE(reader.error().empty());
}

TEST_F(ProcessingPathsTest, SequentialReportsMalformedXmlAndCompressedErrors) {
    DumpReader reader(path);
    for (const auto &data: {bz2("<mediawiki><page><title>unfinished"), first + last.substr(0, last.size() - 1)}) {
        write_file(path.dump_path(), data);
        reader.process_all([](const auto &, const auto &) { return true; });
        EXPECT_FALSE(reader.error().empty());
    }
}

TEST_F(ProcessingPathsTest, SequentialReportsCallbacksAndMissingFile) {
    DumpReader reader(path);
    reader.process_all(nullptr);
    EXPECT_FALSE(reader.error().empty());
    reader.process_all([](const auto &, const auto &) -> bool { throw std::runtime_error("callback failed"); });
    EXPECT_NE(reader.error().find("callback failed"), std::string::npos);
    reader.process_all([](const auto &, const auto &) { return true; },
                       [](const auto &) { throw std::runtime_error("progress failed"); });
    EXPECT_NE(reader.error().find("progress failed"), std::string::npos);
    std::filesystem::remove(path.dump_path());
    reader.process_all([](const auto &, const auto &) { return true; });
    EXPECT_FALSE(reader.error().empty());
}

TEST_F(ProcessingPathsTest, BatchVisitsChunksInOffsetOrderAndPreservesSuccessfulChunks) {
    write_file(path.index_path(), bz2("0:1:WrongFirst\n" + std::to_string(first.size()) + ":3:WrongLast\n"));
    DumpReader reader(path);
    reader.load_index();
    auto results = reader.extract_pages({"WrongLast", "WrongFirst"});
    EXPECT_FALSE(results[0].found);
    EXPECT_FALSE(results[1].found);
    EXPECT_NE(reader.error().find("WrongFirst"), std::string::npos);
    write_file(path.index_path(), bz2("0:1:WrongFirst\n" + std::to_string(first.size()) + ":3:Last\n"));
    reader.load_index();
    results = reader.extract_pages({"Last", "WrongFirst", "Last"});
    EXPECT_TRUE(results[0].found);
    EXPECT_FALSE(results[1].found);
    EXPECT_TRUE(results[2].found);
    EXPECT_EQ(results[0].content, "tail");
    EXPECT_FALSE(reader.error().empty());
}

TEST_F(ProcessingPathsTest, BatchDoesNotPublishPagesFromChunkWithInvalidSuffix) {
    const auto data = bz2("<page><title>A</title><id>1</id><revision><text>valid</text></revision></page>"
                          "<page><title>unfinished");
    write_file(path.dump_path(), data);
    write_file(path.index_path(), bz2("0:1:A\n"));
    DumpReader reader(path);
    reader.load_index();
    auto results = reader.extract_pages({"A", "A"});
    EXPECT_FALSE(results[0].found);
    EXPECT_FALSE(results[1].found);
    EXPECT_FALSE(reader.error().empty());
}

TEST_F(ProcessingPathsTest, ConvenienceXmlExtractionUsesLastRevision) {
    const std::string xml = "<page><title>A</title><revision><text>old</text></revision>"
                            "<revision><text>new<![CDATA[ & data]]> tail</text></revision></page>";
    EXPECT_EQ(extract_page_from_xml("A", xml), "new & data tail");
    ASSERT_EQ(extract_all_from_xml(xml).size(), 1u);
    EXPECT_EQ(extract_all_from_xml(xml)[0].second, "new & data tail");
}

TEST_F(ProcessingPathsTest, SequentialReportsPeriodicProgress) {
    std::string xml = "<mediawiki>";
    for (int i = 1; i <= 1001; ++i)
        xml += "<page><title>P</title><id>" + std::to_string(i) + "</id><revision><text/></revision></page>";
    xml += "</mediawiki>";
    write_file(path.dump_path(), bz2(xml));
    DumpReader reader(path);
    std::vector<DumpReader::ProcessProgress> updates;
    reader.process_all([](const auto &, const auto &) { return true; },
                       [&](const auto &progress) { updates.push_back(progress); });
    ASSERT_TRUE(reader.error().empty()) << reader.error();
    ASSERT_GE(updates.size(), 3u);
    EXPECT_EQ(updates.front().pages_processed, 0u);
    EXPECT_EQ(updates[1].pages_processed, 1000u);
    EXPECT_EQ(updates.back().pages_processed, 1001u);
    EXPECT_EQ(updates.back().bytes_compressed, path.dump_size());
}

} // namespace
