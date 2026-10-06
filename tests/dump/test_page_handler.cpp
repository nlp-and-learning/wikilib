#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include "wikilib/dump/page_handler.h"

using namespace wikilib::dump;

namespace {

PageHandler handler_for(std::string_view xml) {
    return PageHandler(std::make_unique<XmlReader>(XmlReader::from_string(xml)));
}

const std::string header = "<mediawiki><siteinfo><sitename>Test</sitename><dbname>testwiki</dbname>"
                           "<namespaces><namespace key=\"0\"/><namespace key=\"10\">Template</namespace>"
                           "</namespaces></siteinfo>";

std::string page(std::string_view title, std::string_view revisions, std::string_view redirect = "") {
    return "<page><title>" + std::string(title) + "</title><ns>0</ns><id>1</id>" + std::string(redirect) +
           std::string(revisions) + "</page>";
}

TEST(PageHandlerTest, ReadsFirstAndLastPageAndSiteInfo) {
    auto handler = handler_for(header + page("A", "<revision><text>alpha</text></revision>") +
                               page("B", "<revision><text>beta</text></revision>") + "</mediawiki>");
    EXPECT_EQ(handler.site_info().site_name, "Test");
    ASSERT_EQ(handler.site_info().namespaces.size(), 2u);
    EXPECT_TRUE(handler.site_info().namespaces[0].name.empty());
    EXPECT_EQ(handler.site_info().namespaces[1].name, "Template");
    auto first = handler.next_page();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->info.title, "A");
    EXPECT_EQ(first->content(), "alpha");
    auto last = handler.next_page();
    ASSERT_TRUE(last);
    EXPECT_EQ(last->info.title, "B");
    EXPECT_EQ(last->content(), "beta");
    EXPECT_FALSE(handler.next_page());
    EXPECT_EQ(handler.stats().pages_read, 2u);
    EXPECT_TRUE(handler.error().empty());
}

TEST(PageHandlerTest, PreservesWhitespaceAndHandlesRedirectAndEmptyText) {
    auto handler = handler_for(
            header + page("A", "<revision><minor/><comment/><text/></revision>", "<redirect title=\"Target\"/>") +
            page("B", "<revision><text> \n indented\n\t </text></revision>") + "</mediawiki>");
    auto first = handler.next_page();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->info.redirect_target, "Target");
    ASSERT_EQ(first->revisions.size(), 1u);
    EXPECT_TRUE(first->content().empty());
    auto second = handler.next_page();
    ASSERT_TRUE(second);
    EXPECT_EQ(second->content(), " \n indented\n\t ");
    EXPECT_FALSE(handler.next_page());
    EXPECT_TRUE(handler.error().empty());
}

TEST(PageHandlerTest, RetainsAllRevisionsWithoutFilter) {
    auto handler = handler_for(header +
                               page("A", "<revision><id>10</id><text>old</text></revision>"
                                         "<revision><id>11</id><text>new</text></revision>") +
                               "</mediawiki>");
    auto result = handler.next_page();
    ASSERT_TRUE(result);
    ASSERT_EQ(result->revisions.size(), 2u);
    EXPECT_EQ(result->revisions[0].id, 10u);
    EXPECT_EQ(result->latest_revision()->id, 11u);
    EXPECT_EQ(result->info.revision_id, 11u);
}

TEST(PageHandlerTest, ReadsRevisionMetadataAndContributor) {
    auto handler =
            handler_for(header +
                        page("A", "<revision><id>10</id><parentid>9</parentid>"
                                  "<timestamp>2026-10-06T12:00:00Z</timestamp><contributor><username>User</username>"
                                  "<id>123</id></contributor><comment>note</comment><model>wikitext</model>"
                                  "<format>text/x-wiki</format><text>content</text><sha1>hash</sha1></revision>") +
                        "</mediawiki>");
    auto result = handler.next_page();
    ASSERT_TRUE(result);
    ASSERT_EQ(result->revisions.size(), 1u);
    const auto &rev = result->revisions.front();
    EXPECT_EQ(rev.id, 10u);
    EXPECT_EQ(rev.parent_id, 9u);
    EXPECT_EQ(rev.contributor, "User");
    EXPECT_EQ(rev.comment, "note");
    EXPECT_EQ(rev.model, "wikitext");
    EXPECT_EQ(rev.format, "text/x-wiki");
    EXPECT_EQ(rev.sha1, "hash");
    EXPECT_EQ(result->info.timestamp, "2026-10-06T12:00:00Z");
    EXPECT_TRUE(handler.error().empty());
}

TEST(PageHandlerTest, ProcessAppliesRevisionAndRedirectFilters) {
    auto handler =
            handler_for(header +
                        page("A", "<revision><text>old</text></revision>"
                                  "<revision><text>new</text></revision>") +
                        page("Redirect", "<revision><text/></revision>", "<redirect title=\"A\"/>") + "</mediawiki>");
    PageFilter filter;
    filter.include_redirects = false;
    int count = 0;
    handler.process(
            [&](const Page &result) {
                ++count;
                EXPECT_EQ(result.info.title, "A");
                EXPECT_EQ(result.revisions.size(), 1u);
                EXPECT_EQ(result.content(), "new");
                return true;
            },
            filter);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(handler.stats().pages_skipped, 1u);
    EXPECT_TRUE(handler.error().empty());
}

TEST(PageHandlerTest, ReportsBadHeaderMetadata) {
    auto handler = handler_for("<mediawiki><siteinfo><namespace key=\"invalid\"/></siteinfo></mediawiki>");
    EXPECT_FALSE(handler.error().empty());
    EXPECT_FALSE(handler.next_page());
}

TEST(PageHandlerTest, EmptyDumpWithSiteInfoIsValid) {
    auto handler = handler_for(header + "</mediawiki>");
    EXPECT_FALSE(handler.next_page());
    EXPECT_TRUE(handler.error().empty());
    EXPECT_EQ(handler.stats().pages_read, 0u);
}

TEST(PageHandlerTest, AppliesLatestRevisionFilter) {
    const auto xml = header +
                     page("A", "<revision><id>10</id><text>old</text></revision>"
                               "<revision><id>11</id><text>new</text></revision>") +
                     "</mediawiki>";
    for (bool latest: {true, false}) {
        auto handler = handler_for(xml);
        PageFilter filter;
        filter.only_latest_revision = latest;
        auto result = handler.next_page(filter);
        ASSERT_TRUE(result);
        EXPECT_EQ(result->revisions.size(), latest ? 1u : 2u);
        EXPECT_EQ(result->content(), "new");
    }
}

TEST(PageHandlerTest, RejectsIncompletePagesAndReportsXmlError) {
    for (const auto &xml: {header + "<page><title>A</title><revision><text>unfinished",
                           header + page("A", "<revision><text>x</revision>") + "</mediawiki>"}) {
        auto handler = handler_for(xml);
        EXPECT_FALSE(handler.next_page());
        EXPECT_FALSE(handler.error().empty());
        EXPECT_EQ(handler.stats().pages_read, 0u);
    }
}

TEST(PageHandlerTest, ReportsInvalidNumericMetadataWithoutThrowing) {
    auto handler = handler_for("<mediawiki><page><title>A</title><ns>invalid</ns></page></mediawiki>");
    EXPECT_NO_THROW(EXPECT_FALSE(handler.next_page()));
    EXPECT_FALSE(handler.error().empty());
}

TEST(PageHandlerTest, PropagatesBz2Error) {
    auto data = compress_bz2(header + page("A", "<revision><text>x</text></revision>") + "</mediawiki>");
    ASSERT_TRUE(data);
    data->pop_back();
    FILE *file = std::tmpfile();
    ASSERT_NE(file, nullptr);
    std::fwrite(data->data(), 1, data->size(), file);
    std::rewind(file);
    auto xml = std::make_unique<XmlReader>(std::make_unique<Bz2Stream>(file, true));
    PageHandler handler(std::move(xml));
    while (handler.next_page()) {
    }
    EXPECT_NE(handler.error().find("unexpected EOF"), std::string_view::npos);
}

TEST(PageHandlerTest, ReadsLargePageAcrossStreams) {
    const std::string text = " \n" + std::string(150000, 'x') + "\n ";
    const auto xml = header + page("A", "<revision><text>" + text + "</text></revision>") + "</mediawiki>";
    auto first = compress_bz2(xml.substr(0, 65536));
    auto last = compress_bz2(xml.substr(65536));
    ASSERT_TRUE(first && last);
    const auto data = *first + *last;
    FILE *file = std::tmpfile();
    ASSERT_NE(file, nullptr);
    std::fwrite(data.data(), 1, data.size(), file);
    std::rewind(file);
    PageHandler handler(std::make_unique<XmlReader>(std::make_unique<Bz2Stream>(file, true)));
    auto result = handler.next_page();
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->content() == text);
    EXPECT_FALSE(handler.next_page());
    EXPECT_TRUE(handler.error().empty());
}

} // namespace
