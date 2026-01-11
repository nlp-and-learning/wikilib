/**
 * @file test_tokenizer_utils.cpp
 * @brief Tests for tokenizer utility functions (tokens_to_plain_text, strip_comments_and_nowiki)
 */

#include <gtest/gtest.h>
#include "wikilib/markup/tokenizer.h"

using namespace wikilib;
using namespace wikilib::markup;

class TokenizerUtilsTest : public ::testing::Test {
protected:
    std::vector<Token> tokenize(std::string_view input) {
        Tokenizer tok(input);
        return tok.tokenize_all();
    }

    std::string to_plain_text(std::string_view input) {
        auto tokens = tokenize(input);
        return tokens_to_plain_text(tokens);
    }
};

// ============================================================================
// tokens_to_plain_text tests
// ============================================================================

TEST_F(TokenizerUtilsTest, PlainText_Simple) {
    EXPECT_EQ(to_plain_text("Hello World"), "Hello World");
}

TEST_F(TokenizerUtilsTest, PlainText_WithNewlines) {
    EXPECT_EQ(to_plain_text("Line 1\nLine 2"), "Line 1\nLine 2");
}

TEST_F(TokenizerUtilsTest, PlainText_StripsBold) {
    EXPECT_EQ(to_plain_text("This is '''bold''' text"), "This is bold text");
}

TEST_F(TokenizerUtilsTest, PlainText_StripsItalic) {
    EXPECT_EQ(to_plain_text("This is ''italic'' text"), "This is italic text");
}

TEST_F(TokenizerUtilsTest, PlainText_StripsBoldItalic) {
    EXPECT_EQ(to_plain_text("This is '''''bold italic''''' text"), "This is bold italic text");
}

TEST_F(TokenizerUtilsTest, PlainText_StripsLink) {
    // Strips [[ and ]] markup, keeps target text
    EXPECT_EQ(to_plain_text("See [[Article]] here"), "See Article here");
}

TEST_F(TokenizerUtilsTest, PlainText_StripsLinkWithPipe) {
    // Note: This extracts raw text from tokens, NOT display text.
    // Both target and display parts are kept (without | separator).
    // For display text extraction, use AST-level functions.
    EXPECT_EQ(to_plain_text("See [[Article|display text]] here"), "See Articledisplay text here");
}

TEST_F(TokenizerUtilsTest, PlainText_StripsTemplate) {
    // Strips {{ and }}, keeps template name as text
    // Note: Does NOT expand templates - that requires template definitions
    EXPECT_EQ(to_plain_text("Hello {{template}} world"), "Hello template world");
}

TEST_F(TokenizerUtilsTest, PlainText_StripsTemplateWithParams) {
    // All text content preserved (name, param names, values), markup stripped
    EXPECT_EQ(to_plain_text("Hello {{template|param=value}} world"), "Hello templateparamvalue world");
}

// --- HTML tags ---

TEST_F(TokenizerUtilsTest, PlainText_ValidHtmlTag_Span) {
    // <span> is valid HTML tag - should be stripped
    EXPECT_EQ(to_plain_text("Hello <span>styled</span> world"), "Hello styled world");
}

TEST_F(TokenizerUtilsTest, PlainText_ValidHtmlTag_Div) {
    EXPECT_EQ(to_plain_text("Hello <div>block</div> world"), "Hello block world");
}

TEST_F(TokenizerUtilsTest, PlainText_ValidHtmlTag_Strong) {
    EXPECT_EQ(to_plain_text("<strong>important</strong>"), "important");
}

TEST_F(TokenizerUtilsTest, PlainText_ValidHtmlTag_SelfClosing) {
    EXPECT_EQ(to_plain_text("Line 1<br/>Line 2"), "Line 1Line 2");
}

TEST_F(TokenizerUtilsTest, PlainText_InvalidTag_SingleLetter) {
    // <n> is NOT a valid tag - should be preserved as text with < and >
    EXPECT_EQ(to_plain_text("a<n>b</n>c"), "a<n>b</n>c");
}

TEST_F(TokenizerUtilsTest, PlainText_InvalidTag_Unknown) {
    // <xyz> is NOT a valid tag - should be preserved as text
    EXPECT_EQ(to_plain_text("a<xyz>b</xyz>c"), "a<xyz>b</xyz>c");
}

TEST_F(TokenizerUtilsTest, PlainText_InvalidTag_Number) {
    // <1> is NOT a valid tag - should be preserved as text
    EXPECT_EQ(to_plain_text("a<1>b"), "a<1>b");
}

TEST_F(TokenizerUtilsTest, PlainText_LessThanGreaterThan_NoTag) {
    // Just < and > without forming a tag
    EXPECT_EQ(to_plain_text("a < b > c"), "a < b > c");
}

TEST_F(TokenizerUtilsTest, PlainText_MathInequality) {
    // Common case: math expressions
    EXPECT_EQ(to_plain_text("if x < 10 and y > 5"), "if x < 10 and y > 5");
}

TEST_F(TokenizerUtilsTest, PlainText_MixedValidAndInvalid) {
    // Mix of valid tag and invalid tag-like text
    EXPECT_EQ(to_plain_text("<b>bold</b> and <x>not a tag</x>"), "bold and <x>not a tag</x>");
}

TEST_F(TokenizerUtilsTest, PlainText_NestedTags) {
    EXPECT_EQ(to_plain_text("<div><span>nested</span></div>"), "nested");
}

// --- Edge cases ---

TEST_F(TokenizerUtilsTest, PlainText_Empty) {
    EXPECT_EQ(to_plain_text(""), "");
}

TEST_F(TokenizerUtilsTest, PlainText_OnlyMarkup) {
    // Only markup, no text content
    EXPECT_EQ(to_plain_text("[[]]"), "");
}

TEST_F(TokenizerUtilsTest, PlainText_Complex) {
    std::string input = "== Heading ==\n'''Bold''' and ''italic'' with [[link|text]] and {{template}}.";
    std::string expected = " Heading \nBold and italic with linktext and template.";
    EXPECT_EQ(to_plain_text(input), expected);
}