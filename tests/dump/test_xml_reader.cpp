#include <cstdio>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include "wikilib/dump/xml_reader.h"

using namespace wikilib::dump;

namespace {

std::unique_ptr<XmlReader> compressed_reader(const std::string &data) {
    FILE *file = std::tmpfile();
    if (!file)
        throw std::runtime_error("Cannot create test file");
    if (std::fwrite(data.data(), 1, data.size(), file) != data.size()) {
        std::fclose(file);
        throw std::runtime_error("Cannot write test file");
    }
    std::rewind(file);
    return std::make_unique<XmlReader>(std::make_unique<Bz2Stream>(file, true));
}

TEST(XmlReaderTest, SelfClosingElementsHavePairedEventsAndPaths) {
    auto reader = XmlReader::from_string("<root><empty key=\"value\"/><next/></root>");
    ASSERT_EQ(reader.next()->type, XmlEventType::StartDocument);
    ASSERT_EQ(reader.next()->name, "root");
    auto event = reader.next();
    ASSERT_TRUE(event);
    EXPECT_EQ(event->name, "empty");
    EXPECT_EQ(event->get_attribute("key"), "value");
    EXPECT_EQ(reader.current_path(), "root/empty");
    event = reader.next();
    EXPECT_EQ(event->type, XmlEventType::EndElement);
    EXPECT_EQ(event->name, "empty");
    EXPECT_EQ(reader.current_path(), "root");
    EXPECT_EQ(reader.next()->name, "next");
    EXPECT_EQ(reader.next()->name, "next");
    EXPECT_EQ(reader.next()->name, "root");
    EXPECT_EQ(reader.next()->type, XmlEventType::EndDocument);
    EXPECT_FALSE(reader.next());
    EXPECT_TRUE(reader.error().empty());
}

TEST(XmlReaderTest, ReadTextPreservesWhitespaceAndDecodesEntities) {
    auto reader = XmlReader::from_string("<text> \n\t indented &lt;x&gt; &amp; &#65; &#x1F642;  \n</text>");
    (void) reader.next();
    (void) reader.next();
    EXPECT_EQ(reader.read_text(), " \n\t indented <x> & A 🙂  \n");
    EXPECT_TRUE(reader.error().empty());
}

TEST(XmlReaderTest, EmptyTextDoesNotConsumeFollowingElement) {
    auto reader = XmlReader::from_string("<root><text/><next>value</next></root>");
    (void) reader.next();
    (void) reader.next();
    (void) reader.next();
    EXPECT_EQ(reader.read_text(), "");
    EXPECT_EQ(reader.next()->name, "next");
    EXPECT_EQ(reader.read_text(), "value");
}

TEST(XmlReaderTest, SkipSelfClosingElementDoesNotSkipSibling) {
    auto reader = XmlReader::from_string("<root><empty/><next/></root>");
    (void) reader.next();
    (void) reader.next();
    (void) reader.next();
    reader.skip_element();
    EXPECT_EQ(reader.next()->name, "next");
}

TEST(XmlReaderTest, ReadsTagsAttributesAndTextAcrossBuffersAndStreams) {
    const std::string prefix = "<root>" + std::string(65529, ' ');
    const std::string xml = prefix + "<text key=\"" + std::string(65540, 'a') + "\"> \n" + std::string(131073, 'x') +
                            " &amp; </text></root>";
    const auto first = compress_bz2(xml.substr(0, 65536));
    const auto second = compress_bz2(xml.substr(65536));
    ASSERT_TRUE(first && second);
    auto reader = compressed_reader(*first + *second);
    bool found = false;
    while (auto event = reader->next()) {
        if (event->type == XmlEventType::StartElement && event->name == "text") {
            found = true;
            const auto key = event->get_attribute("key");
            EXPECT_TRUE(key && *key == std::string(65540, 'a'));
            EXPECT_TRUE(reader->read_text() == " \n" + std::string(131073, 'x') + " & ");
        }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(reader->error().empty());
}

TEST(XmlReaderTest, ReportsMalformedAndTruncatedXml) {
    for (const auto *xml: {"<root><child></root>", "<root><child>", "<root key=\"unfinished", "<root",
                           "<root><!-- unfinished", "<root></root><extra/>", "<root key=value/>",
                           "<root>&#999999999999999999999;</root>", "<root>&#xD800;</root>", "<root>&#0;</root>",
                           "<root>&unknown;</root>", "<root>&amp</root>", "<root key=\"x\"key=\"y\"/>", "<1root/>"}) {
        SCOPED_TRACE(xml);
        auto reader = XmlReader::from_string(xml);
        int errors = 0;
        int ends = 0;
        for (int i = 0; i < 30; ++i) {
            auto event = reader.next();
            if (!event)
                break;
            errors += event->type == XmlEventType::Error;
            ends += event->type == XmlEventType::EndDocument;
        }
        EXPECT_EQ(errors, 1);
        EXPECT_EQ(ends, 0);
        EXPECT_FALSE(reader.error().empty());
        EXPECT_FALSE(reader.next());
    }
}

TEST(XmlReaderTest, ReportsTruncatedTextEvenWhenDrivenByEof) {
    auto reader = XmlReader::from_string("<root>unfinished");
    int errors = 0;
    while (!reader.eof()) {
        auto event = reader.next();
        ASSERT_TRUE(event);
        errors += event->type == XmlEventType::Error;
    }
    EXPECT_EQ(errors, 1);
    EXPECT_FALSE(reader.error().empty());
}

TEST(XmlReaderTest, PropagatesDecompressionFailure) {
    auto data = compress_bz2("<root><text>hello</text></root>");
    ASSERT_TRUE(data);
    data->pop_back();
    auto reader = compressed_reader(*data);
    while (reader->next()) {
    }
    EXPECT_NE(reader->error().find("unexpected EOF"), std::string_view::npos);
}

TEST(XmlReaderTest, PreservesCdataContent) {
    auto reader = XmlReader::from_string("<text><![CDATA[  <x>&raw;\n ]]></text>");
    (void) reader.next();
    (void) reader.next();
    EXPECT_EQ(reader.read_text(), "  <x>&raw;\n ");
    EXPECT_TRUE(reader.error().empty());
}

TEST(XmlReaderTest, ElementIteratorOwnsNamesAndAttributes) {
    std::optional<XmlElementIterator::Element> saved;
    {
        auto reader = XmlReader::from_string("<root><item key=\"first\">one</item>"
                                             "<item key=\"second\">two</item></root>");
        XmlElementIterator iterator(reader, "root/item");
        auto first = iterator.next();
        ASSERT_TRUE(first);
        EXPECT_EQ(first->name, "item");
        EXPECT_EQ(first->attribute("key"), "first");
        saved = *first;
        auto second = iterator.next();
        ASSERT_TRUE(second);
        EXPECT_EQ(second->attribute("key"), "second");
        EXPECT_EQ(first->attribute("key"), "first");
    }
    EXPECT_EQ(saved->name, "item");
    EXPECT_EQ(saved->attribute("key"), "first");
    EXPECT_EQ(saved->text_content, "one");
}

TEST(XmlReaderTest, ReadsDeclarationAndBom) {
    auto reader = XmlReader::from_string("\xEF\xBB\xBF<?xml version=\"1.0\"?>\n<root/>");
    ASSERT_EQ(reader.next()->type, XmlEventType::StartDocument);
    ASSERT_EQ(reader.next()->name, "root");
    ASSERT_EQ(reader.next()->type, XmlEventType::EndElement);
    EXPECT_EQ(reader.next()->type, XmlEventType::EndDocument);
    EXPECT_TRUE(reader.error().empty());
}

} // namespace
