#include <gtest/gtest.h>
#include "wikilib/markup/tokenizer.h"

using namespace wikilib;
using namespace wikilib::markup;

class TokenizerTest : public ::testing::Test {
protected:
    std::vector<Token> tokenize(std::string_view input) {
        Tokenizer tok(input);
        return tok.tokenize_all();
    }

    Token first_token(std::string_view input) {
        Tokenizer tok(input);
        return tok.next();
    }
};

TEST_F(TokenizerTest, PlainText) {
    auto tok = first_token("Hello World");
    EXPECT_EQ(tok.type, TokenType::Text);
    EXPECT_EQ(tok.text, "Hello World");
}

TEST_F(TokenizerTest, Bold) {
    auto tokens = tokenize("'''bold'''");
    ASSERT_GE(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::Bold);
    EXPECT_EQ(tokens[1].type, TokenType::Text);
    EXPECT_EQ(tokens[1].text, "bold");
    EXPECT_EQ(tokens[2].type, TokenType::Bold);
}

TEST_F(TokenizerTest, Italic) {
    auto tokens = tokenize("''italic''");
    ASSERT_GE(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::Italic);
    EXPECT_EQ(tokens[1].text, "italic");
    EXPECT_EQ(tokens[2].type, TokenType::Italic);
}

TEST_F(TokenizerTest, InternalLink) {
    auto tokens = tokenize("[[Page name]]");
    ASSERT_GE(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::LinkOpen);
    EXPECT_EQ(tokens[1].type, TokenType::Text);
    EXPECT_EQ(tokens[1].text, "Page name");
    EXPECT_EQ(tokens[2].type, TokenType::LinkClose);
}

TEST_F(TokenizerTest, PipedLink) {
    auto tokens = tokenize("[[Page|Display text]]");
    ASSERT_GE(tokens.size(), 5u);
    EXPECT_EQ(tokens[0].type, TokenType::LinkOpen);
    EXPECT_EQ(tokens[1].text, "Page");
    EXPECT_EQ(tokens[2].type, TokenType::LinkSeparator);
    EXPECT_EQ(tokens[3].text, "Display text");
    EXPECT_EQ(tokens[4].type, TokenType::LinkClose);
}

TEST_F(TokenizerTest, Template) {
    auto tokens = tokenize("{{Template}}");
    ASSERT_GE(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::TemplateOpen);
    EXPECT_EQ(tokens[1].text, "Template");
    EXPECT_EQ(tokens[2].type, TokenType::TemplateClose);
}

TEST_F(TokenizerTest, TemplateWithParams) {
    auto tokens = tokenize("{{Tpl|param1|param2}}");
    EXPECT_EQ(tokens[0].type, TokenType::TemplateOpen);
    // Find pipes
    int pipes = 0;
    for (const auto &t: tokens) {
        if (t.type == TokenType::Pipe)
            pipes++;
    }
    EXPECT_EQ(pipes, 2);
}

TEST_F(TokenizerTest, TemplateWithNamedParams) {
    auto tokens = tokenize("{{Tpl|name=value}}");
    bool found_equals = false;
    for (const auto &t: tokens) {
        if (t.type == TokenType::Equals) {
            found_equals = true;
            break;
        }
    }
    EXPECT_TRUE(found_equals);
}

TEST_F(TokenizerTest, Parameter) {
    auto tokens = tokenize("{{{param}}}");
    ASSERT_GE(tokens.size(), 3u);
    EXPECT_EQ(tokens[0].type, TokenType::ParameterOpen);
    EXPECT_EQ(tokens[1].text, "param");
    EXPECT_EQ(tokens[2].type, TokenType::ParameterClose);
}

TEST_F(TokenizerTest, Heading) {
    auto tok = first_token("== Section ==");
    EXPECT_EQ(tok.type, TokenType::Heading);
    EXPECT_EQ(tok.level, 2);
}

TEST_F(TokenizerTest, HeadingLevels) {
    for (int level = 1; level <= 6; level++) {
        std::string input(level, '=');
        input += " Test ";
        input += std::string(level, '=');
        auto tok = first_token(input);
        EXPECT_EQ(tok.type, TokenType::Heading);
        EXPECT_EQ(tok.level, level);
    }
}

TEST_F(TokenizerTest, BulletList) {
    auto tok = first_token("* Item");
    EXPECT_EQ(tok.type, TokenType::BulletList);
    EXPECT_EQ(tok.level, 1);
}

TEST_F(TokenizerTest, NestedList) {
    auto tok = first_token("*** Deep item");
    EXPECT_EQ(tok.type, TokenType::BulletList);
    EXPECT_EQ(tok.level, 3);
}

TEST_F(TokenizerTest, NumberedList) {
    auto tok = first_token("# Item");
    EXPECT_EQ(tok.type, TokenType::NumberedList);
}

TEST_F(TokenizerTest, TableStart) {
    auto tok = first_token("{|");
    EXPECT_EQ(tok.type, TokenType::TableStart);
}

TEST_F(TokenizerTest, TableEnd) {
    auto tokens = tokenize("{|\n|}");
    bool found_end = false;
    for (const auto &t: tokens) {
        if (t.type == TokenType::TableEnd) {
            found_end = true;
            break;
        }
    }
    EXPECT_TRUE(found_end);
}

TEST_F(TokenizerTest, HtmlComment) {
    Tokenizer tok("<!-- comment -->text", {.preserve_comments = true});
    auto t1 = tok.next();
    EXPECT_EQ(t1.type, TokenType::HtmlComment);
    auto t2 = tok.next();
    EXPECT_EQ(t2.type, TokenType::Text);
}

TEST_F(TokenizerTest, NoWiki) {
    auto tokens = tokenize("<nowiki>'''not bold'''</nowiki>");
    bool found_nowiki = false;
    for (const auto &t: tokens) {
        if (t.type == TokenType::NoWiki) {
            found_nowiki = true;
            EXPECT_EQ(t.text, "'''not bold'''");
            break;
        }
    }
    EXPECT_TRUE(found_nowiki);
}

TEST_F(TokenizerTest, HorizontalRule) {
    auto tok = first_token("----");
    EXPECT_EQ(tok.type, TokenType::HorizontalRule);
}

TEST_F(TokenizerTest, ExternalLink) {
    auto tokens = tokenize("[https://example.com Example]");
    EXPECT_EQ(tokens[0].type, TokenType::ExternalLinkOpen);
}

TEST_F(TokenizerTest, MixedContent) {
    auto tokens = tokenize("Hello '''bold''' and [[link]]");
    EXPECT_GT(tokens.size(), 5u);

    // Should have text, bold markers, more text, link markers
    bool has_text = false, has_bold = false, has_link = false;
    for (const auto &t: tokens) {
        if (t.type == TokenType::Text)
            has_text = true;
        if (t.type == TokenType::Bold)
            has_bold = true;
        if (t.type == TokenType::LinkOpen)
            has_link = true;
    }
    EXPECT_TRUE(has_text);
    EXPECT_TRUE(has_bold);
    EXPECT_TRUE(has_link);
}

TEST_F(TokenizerTest, SourceLocation) {
    Tokenizer tok("Hello\nWorld");
    auto t1 = tok.next();
    EXPECT_EQ(t1.location.begin.line, 1u);
    EXPECT_EQ(t1.location.begin.column, 1u);
}

TEST_F(TokenizerTest, LooksLikeWikitext) {
    EXPECT_TRUE(looks_like_wikitext("Hello [[world]]"));
    EXPECT_TRUE(looks_like_wikitext("{{template}}"));
    EXPECT_TRUE(looks_like_wikitext("'''bold'''"));
    EXPECT_FALSE(looks_like_wikitext("Plain text"));
    EXPECT_FALSE(looks_like_wikitext("No markup here"));
}
