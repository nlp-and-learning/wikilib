#include <gtest/gtest.h>
#include "wikilib/core/text_utils.hpp"

using namespace wikilib;

TEST(TextUtilsTest, TrimLeft) {
    EXPECT_EQ(text::trim_left("  hello"), "hello");
    EXPECT_EQ(text::trim_left("hello"), "hello");
    EXPECT_EQ(text::trim_left(""), "");
    EXPECT_EQ(text::trim_left("  "), "");
    EXPECT_EQ(text::trim_left("\t\nhello"), "hello");
}

TEST(TextUtilsTest, TrimRight) {
    EXPECT_EQ(text::trim_right("hello  "), "hello");
    EXPECT_EQ(text::trim_right("hello"), "hello");
    EXPECT_EQ(text::trim_right(""), "");
    EXPECT_EQ(text::trim_right("hello\n\t"), "hello");
}

TEST(TextUtilsTest, Trim) {
    EXPECT_EQ(text::trim("  hello  "), "hello");
    EXPECT_EQ(text::trim("hello"), "hello");
    EXPECT_EQ(text::trim(""), "");
    EXPECT_EQ(text::trim("  "), "");
}

TEST(TextUtilsTest, CollapseWhitespace) {
    EXPECT_EQ(text::collapse_whitespace("hello   world"), "hello world");
    EXPECT_EQ(text::collapse_whitespace("  hello  world  "), " hello world ");
    EXPECT_EQ(text::collapse_whitespace("a\t\nb"), "a b");
}

TEST(TextUtilsTest, ToLowerAscii) {
    std::string s = "Hello World";
    text::to_lower_ascii(s);
    EXPECT_EQ(s, "hello world");

    EXPECT_EQ(text::to_lower_ascii_copy("HELLO"), "hello");
    EXPECT_EQ(text::to_lower_ascii_copy("123"), "123");
}

TEST(TextUtilsTest, EqualsIgnoreCaseAscii) {
    EXPECT_TRUE(text::equals_ignore_case_ascii("Hello", "hello"));
    EXPECT_TRUE(text::equals_ignore_case_ascii("HELLO", "hello"));
    EXPECT_FALSE(text::equals_ignore_case_ascii("Hello", "world"));
    EXPECT_FALSE(text::equals_ignore_case_ascii("Hello", "Hell"));
}

TEST(TextUtilsTest, StartsWithEndsWith) {
    EXPECT_TRUE(text::starts_with("hello world", "hello"));
    EXPECT_FALSE(text::starts_with("hello world", "world"));

    EXPECT_TRUE(text::ends_with("hello world", "world"));
    EXPECT_FALSE(text::ends_with("hello world", "hello"));
}

TEST(TextUtilsTest, Split) {
    auto parts = text::split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");

    auto parts2 = text::split("a||b", "||");
    ASSERT_EQ(parts2.size(), 2u);
    EXPECT_EQ(parts2[0], "a");
    EXPECT_EQ(parts2[1], "b");
}

TEST(TextUtilsTest, SplitN) {
    auto parts = text::split_n("a,b,c,d", ',', 2);
    ASSERT_EQ(parts.size(), 2u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b,c,d");
}

TEST(TextUtilsTest, ReplaceAll) {
    EXPECT_EQ(text::replace_all("hello world", "o", "0"), "hell0 w0rld");
    EXPECT_EQ(text::replace_all("aaa", "a", "bb"), "bbbbbb");
    EXPECT_EQ(text::replace_all("test", "x", "y"), "test");
}

TEST(TextUtilsTest, CountOccurrences) {
    EXPECT_EQ(text::count_occurrences("hello world", "o"), 2u);
    EXPECT_EQ(text::count_occurrences("aaa", "aa"), 1u); // Non-overlapping
    EXPECT_EQ(text::count_occurrences("test", "x"), 0u);
}

TEST(TextUtilsTest, ParseInt) {
    EXPECT_EQ(text::parse_int<int>("123"), 123);
    EXPECT_EQ(text::parse_int<int>("-42"), -42);
    EXPECT_EQ(text::parse_int<int>("  456  "), 456);
    EXPECT_EQ(text::parse_int<int>("abc"), std::nullopt);
    EXPECT_EQ(text::parse_int<int>(""), std::nullopt);
}

TEST(TextUtilsTest, DecodeHtmlEntities) {
    EXPECT_EQ(text::decode_html_entities("&amp;"), "&");
    EXPECT_EQ(text::decode_html_entities("&lt;&gt;"), "<>");
    EXPECT_EQ(text::decode_html_entities("&quot;hello&quot;"), "\"hello\"");
    EXPECT_EQ(text::decode_html_entities("&#60;"), "<");
    EXPECT_EQ(text::decode_html_entities("&#x3C;"), "<");
}

TEST(TextUtilsTest, SplitLines) {
    auto lines = text::split_lines("a\nb\nc");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "b");
    EXPECT_EQ(lines[2], "c");

    auto lines2 = text::split_lines("a\r\nb\rc");
    ASSERT_EQ(lines2.size(), 3u);
}

TEST(TextUtilsTest, GetLine) {
    auto line = text::get_line("a\nb\nc", 1);
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(*line, "b");

    EXPECT_FALSE(text::get_line("a\nb", 5).has_value());
}
