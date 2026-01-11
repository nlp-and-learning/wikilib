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

// ============================================================================
// NoWiki and Comment interaction tests
// Based on z_innego_projektu/test/CommentTest.cpp and NowikiTest.cpp
// ============================================================================

// --- Comment tests ---

TEST_F(TokenizerTest, Comment_HideComments) {
    // "This is<!--comment 1--> simple <!--comment 2-->text" -> "This is simple text"
    std::string result = strip_comments_and_nowiki("This is<!--comment 1--> simple <!--comment 2-->text");
    EXPECT_EQ(result, "This is simple text");
}

TEST_F(TokenizerTest, Comment_AtEnd) {
    // "This is simple text<!--comment" -> "This is simple text"
    // Unclosed comment consumes rest of input
    std::string result = strip_comments_and_nowiki("This is simple text<!--comment");
    EXPECT_EQ(result, "This is simple text");
}

TEST_F(TokenizerTest, Comment_AtEnd2) {
    // "This is simple --> text<!--comment" -> "This is simple --> text"
    // Standalone --> before any <!-- is just text
    std::string result = strip_comments_and_nowiki("This is simple --> text<!--comment");
    EXPECT_EQ(result, "This is simple --> text");
}

TEST_F(TokenizerTest, Comment_InsideNowiki) {
    // "<nowiki><!--comment--></nowiki>" preserves comment as text
    std::string result = strip_comments_and_nowiki("<nowiki><!--comment--></nowiki>");
    EXPECT_EQ(result, "<!--comment-->");
}

TEST_F(TokenizerTest, Comment_InsideNowikiWithText) {
    // "This is <nowiki><!--comment--></nowiki> text" -> "This is <!--comment--> text"
    std::string result = strip_comments_and_nowiki("This is <nowiki><!--comment--></nowiki> text");
    EXPECT_EQ(result, "This is <!--comment--> text");
}

TEST_F(TokenizerTest, Comment_NowikiInsideComment) {
    // "This is <!--comment <nowiki> abc <nowiki> --> text" -> "This is  text"
    // <nowiki> inside comment is ignored, comment ends at first -->
    std::string result = strip_comments_and_nowiki("This is <!--comment <nowiki> abc <nowiki> --> text");
    EXPECT_EQ(result, "This is  text");
}

// --- NoWiki tests ---

TEST_F(TokenizerTest, NoWiki_CommentInsideNowiki) {
    // "<nowiki><!-- comment--></nowiki>" -> "<!-- comment-->"
    std::string result = strip_comments_and_nowiki("<nowiki><!-- comment--></nowiki>");
    EXPECT_EQ(result, "<!-- comment-->");
}

TEST_F(TokenizerTest, NoWiki_NotClosed) {
    // "<nowiki><!-- comment--><nowiki>" -> unclosed nowiki, content until end
    // When <nowiki> is not closed, tokenizer should treat it as text
    auto tokens = tokenize("<nowiki><!-- comment--><nowiki>");

    // Check that we have a NoWiki token with content up to end
    bool found_nowiki = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::NoWiki) {
            found_nowiki = true;
            // Content should include everything until EOF (no closing tag found)
            EXPECT_EQ(t.text, "<!-- comment--><nowiki>");
        }
    }
    EXPECT_TRUE(found_nowiki);
}

TEST_F(TokenizerTest, NoWiki_SecondOpen) {
    // "<nowiki><!-- comment--><nowiki></nowiki>" -> "<!-- comment--><nowiki>"
    // First <nowiki> is closed by first </nowiki>, second <nowiki> is included as text
    std::string result = strip_comments_and_nowiki("<nowiki><!-- comment--><nowiki></nowiki>");
    EXPECT_EQ(result, "<!-- comment--><nowiki>");
}

TEST_F(TokenizerTest, NoWiki_Multi) {
    // "abc<nowiki>a</nowiki><nowiki>a</nowiki>b<nowiki>c</nowiki><nowiki>d"
    // Multiple nowiki blocks, last one unclosed
    std::string result = strip_comments_and_nowiki("abc<nowiki>a</nowiki><nowiki>a</nowiki>b<nowiki>c</nowiki><nowiki>d");
    // Last <nowiki>d has no closing tag, so content is "d" until EOF
    EXPECT_EQ(result, "abcaabcd");
}

TEST_F(TokenizerTest, NoWiki_StartPlusNowiki) {
    // "abc<nowiki>a</nowiki>" -> "abca"
    std::string result = strip_comments_and_nowiki("abc<nowiki>a</nowiki>");
    EXPECT_EQ(result, "abca");
}

TEST_F(TokenizerTest, NoWiki_Templates) {
    // "{{name|a|b}}<nowiki>{{name|a|b}}</nowiki>"
    // First template is parsed as template, second is literal text in nowiki
    auto tokens = tokenize("{{name|a|b}}<nowiki>{{name|a|b}}</nowiki>");

    // Check that we have both a template and nowiki
    bool found_template = false;
    bool found_nowiki = false;
    std::string nowiki_content;

    for (const auto& t : tokens) {
        if (t.type == TokenType::TemplateOpen) found_template = true;
        if (t.type == TokenType::NoWiki) {
            found_nowiki = true;
            nowiki_content = std::string(t.text);
        }
    }

    EXPECT_TRUE(found_template);
    EXPECT_TRUE(found_nowiki);
    EXPECT_EQ(nowiki_content, "{{name|a|b}}");
}

// --- Edge cases ---

TEST_F(TokenizerTest, NoWiki_SelfClosing) {
    // "<nowiki/>" is self-closing, no content
    std::string result = strip_comments_and_nowiki("abc<nowiki/>def");
    EXPECT_EQ(result, "abcdef");
}

TEST_F(TokenizerTest, Comment_PreserveMode) {
    // With preserve_comments = true, comment should appear as token
    Tokenizer tok("text<!-- comment -->more", {.preserve_comments = true});
    auto tokens = tok.tokenize_all();

    bool found_comment = false;
    std::string comment_text;
    for (const auto& t : tokens) {
        if (t.type == TokenType::HtmlComment) {
            found_comment = true;
            comment_text = std::string(t.text);
        }
    }

    EXPECT_TRUE(found_comment);
    EXPECT_EQ(comment_text, "<!-- comment -->");
}

TEST_F(TokenizerTest, Comment_PartInsideNowiki) {
    // "This is <nowiki><!--comm</nowiki>ent--> text"
    // Opening <!-- is inside nowiki so it's not a comment start
    // After </nowiki>, "ent-->" is just text (no matching <!--)
    std::string result = strip_comments_and_nowiki("This is <nowiki><!--comm</nowiki>ent--> text");
    EXPECT_EQ(result, "This is <!--comment--> text");
}

TEST_F(TokenizerTest, NoWiki_InsideCommentPart) {
    // "This is <!--comm<nowiki>>ent--></nowiki text"
    // <!-- starts comment, looks for --> which is at "ent-->"
    // So comment content is "comm<nowiki>>ent"
    // Then "</nowiki text" remains
    std::string result = strip_comments_and_nowiki("This is <!--comm<nowiki>>ent--></nowiki text");
    EXPECT_EQ(result, "This is </nowiki text");
}

// ============================================================================
// HTML tag validation tests
// Based on z_innego_projektu/test_tag_parsing.cpp
// ============================================================================

TEST_F(TokenizerTest, Tag_ValidSpan) {
    // "span" is a valid tag name
    auto tokens = tokenize("<span>");

    bool found_tag = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::HtmlTagOpen) {
            found_tag = true;
            EXPECT_EQ(t.tag_name, "span");
        }
    }
    EXPECT_TRUE(found_tag);
}

TEST_F(TokenizerTest, Tag_ValidSub) {
    // "sub" is a valid tag name
    auto tokens = tokenize("<sub>");

    bool found_tag = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::HtmlTagOpen) {
            found_tag = true;
            EXPECT_EQ(t.tag_name, "sub");
        }
    }
    EXPECT_TRUE(found_tag);
}

TEST_F(TokenizerTest, Tag_ValidClosing) {
    // "</sub>" is a valid closing tag
    auto tokens = tokenize("</sub>");

    bool found_tag = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::HtmlTagClose) {
            found_tag = true;
            EXPECT_EQ(t.tag_name, "sub");
        }
    }
    EXPECT_TRUE(found_tag);
}

TEST_F(TokenizerTest, Tag_ValidSelfClosing) {
    // "<br />" is a valid self-closing tag
    auto tokens = tokenize("<br />");

    bool found_tag = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::HtmlTagOpen) {
            found_tag = true;
            EXPECT_EQ(t.tag_name, "br");
            EXPECT_TRUE(t.self_closing);
        }
    }
    EXPECT_TRUE(found_tag);
}

TEST_F(TokenizerTest, Tag_InvalidSingleLetter) {
    // "n" is NOT a valid tag name - should be treated as text
    auto tokens = tokenize("m<n>a</n>");

    // Should not find any HTML tags
    bool found_tag = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::HtmlTagOpen || t.type == TokenType::HtmlTagClose) {
            found_tag = true;
        }
    }
    EXPECT_FALSE(found_tag);

    // Should just be text with < and > characters
    std::string result = strip_comments_and_nowiki("m<n>a</n>");
    EXPECT_EQ(result, "m<n>a</n>");
}

TEST_F(TokenizerTest, Tag_InvalidName) {
    // "xyz" is NOT a valid tag name
    auto tokens = tokenize("<xyz>content</xyz>");

    // Should not find any HTML tags
    bool found_tag = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::HtmlTagOpen || t.type == TokenType::HtmlTagClose) {
            found_tag = true;
        }
    }
    EXPECT_FALSE(found_tag);
}

TEST_F(TokenizerTest, Tag_ValidInText) {
    // Based on test_tag_parsing.cpp: ParseValidTagInText
    // "m<span && m>a </span>" - span is valid
    auto tokens = tokenize("m<span && m>a </span>");

    bool found_opening = false;
    bool found_closing = false;

    for (const auto& t : tokens) {
        if (t.type == TokenType::HtmlTagOpen && t.tag_name == "span") {
            found_opening = true;
        }
        if (t.type == TokenType::HtmlTagClose && t.tag_name == "span") {
            found_closing = true;
        }
    }

    EXPECT_TRUE(found_opening);
    EXPECT_TRUE(found_closing);
}

TEST_F(TokenizerTest, Tag_InvalidInText) {
    // Based on test_tag_parsing.cpp: ParseInvalidNonTagInText
    // "m<n && m>a </n>" - n is NOT valid
    auto tokens = tokenize("m<n && m>a </n>");

    // Should not find any HTML tags
    bool found_tag = false;
    for (const auto& t : tokens) {
        if (t.type == TokenType::HtmlTagOpen || t.type == TokenType::HtmlTagClose) {
            found_tag = true;
        }
    }
    EXPECT_FALSE(found_tag);
}

TEST_F(TokenizerTest, Tag_AllValidTags) {
    // Test a sample of valid tags
    std::vector<std::string> valid_tags = {
        "abbr", "b", "div", "span", "strong", "em", "code", "pre",
        "h1", "h2", "h3", "p", "br", "hr", "table", "tr", "td", "th"
    };

    for (const auto& tag : valid_tags) {
        std::string input = "<" + tag + ">";
        auto tokens = tokenize(input);

        bool found = false;
        for (const auto& t : tokens) {
            if (t.type == TokenType::HtmlTagOpen && t.tag_name == tag) {
                found = true;
                break;
            }
        }

        EXPECT_TRUE(found) << "Tag '" << tag << "' should be recognized as valid";
    }
}

TEST_F(TokenizerTest, Tag_MixedValidInvalid) {
    // Mix of valid and invalid tags
    auto tokens = tokenize("text <span>valid</span> more <xyz>invalid</xyz> end");

    int valid_tags = 0;
    int invalid_tags = 0;

    for (const auto& t : tokens) {
        if (t.type == TokenType::HtmlTagOpen || t.type == TokenType::HtmlTagClose) {
            if (t.tag_name == "span") {
                valid_tags++;
            } else if (t.tag_name == "xyz") {
                invalid_tags++;
            }
        }
    }

    EXPECT_EQ(valid_tags, 2);  // <span> and </span>
    EXPECT_EQ(invalid_tags, 0); // xyz should not be recognized as tag
}
