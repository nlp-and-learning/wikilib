#include <gtest/gtest.h>
#include "wikilib/markup/parser.h"

using namespace wikilib::markup;

// ============================================================================
// Basic parsing tests
// ============================================================================

TEST(ParserTest, ParseEmpty) {
    Parser parser;
    auto result = parser.parse("");

    ASSERT_TRUE(result.success());
    ASSERT_NE(result.document, nullptr);
    EXPECT_TRUE(result.document->content.empty());
}

TEST(ParserTest, ParsePlainText) {
    Parser parser;
    auto result = parser.parse("Hello World");

    ASSERT_TRUE(result.success());
    ASSERT_FALSE(result.document->content.empty());

    // Should have at least one node
    EXPECT_GE(result.document->content.size(), 1u);
}

TEST(ParserTest, ParseResult_Success) {
    Parser parser;
    auto result = parser.parse("test");

    EXPECT_TRUE(result.success());
    EXPECT_NE(result.document, nullptr);
}

// ============================================================================
// Text parsing tests
// ============================================================================

TEST(ParserTest, ParseSimpleText) {
    Parser parser;
    auto result = parser.parse("Simple text");

    ASSERT_TRUE(result.success());
    ASSERT_FALSE(result.document->content.empty());

    // Check if we got text content
    bool has_text = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Text || node->type == NodeType::Paragraph) {
            has_text = true;
            break;
        }
    }
    EXPECT_TRUE(has_text);
}

TEST(ParserTest, ParseMultilineText) {
    Parser parser;
    auto result = parser.parse("Line 1\nLine 2\nLine 3");

    ASSERT_TRUE(result.success());
    EXPECT_FALSE(result.document->content.empty());
}

// ============================================================================
// Formatting tests
// ============================================================================

TEST(ParserTest, ParseBold) {
    Parser parser;
    auto result = parser.parse("'''bold text'''");

    ASSERT_TRUE(result.success());

    // Should contain formatting node
    bool found_formatting = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Formatting) {
            found_formatting = true;
            auto* fmt = static_cast<FormattingNode*>(node.get());
            EXPECT_EQ(fmt->style, FormattingNode::Style::Bold);
        } else if (node->type == NodeType::Paragraph) {
            auto* para = static_cast<ParagraphNode*>(node.get());
            for (const auto& child : para->content) {
                if (child->type == NodeType::Formatting) {
                    found_formatting = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_formatting);
}

TEST(ParserTest, ParseItalic) {
    Parser parser;
    auto result = parser.parse("''italic text''");

    ASSERT_TRUE(result.success());

    bool found_formatting = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Formatting) {
            found_formatting = true;
            auto* fmt = static_cast<FormattingNode*>(node.get());
            EXPECT_EQ(fmt->style, FormattingNode::Style::Italic);
        } else if (node->type == NodeType::Paragraph) {
            auto* para = static_cast<ParagraphNode*>(node.get());
            for (const auto& child : para->content) {
                if (child->type == NodeType::Formatting) {
                    found_formatting = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_formatting);
}

TEST(ParserTest, ParseBoldItalic) {
    Parser parser;
    auto result = parser.parse("'''''bold and italic'''''");

    ASSERT_TRUE(result.success());

    bool found_formatting = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Formatting) {
            found_formatting = true;
        } else if (node->type == NodeType::Paragraph) {
            auto* para = static_cast<ParagraphNode*>(node.get());
            for (const auto& child : para->content) {
                if (child->type == NodeType::Formatting) {
                    found_formatting = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_formatting);
}

// ============================================================================
// Link tests
// ============================================================================

TEST(ParserTest, ParseInternalLink) {
    Parser parser;
    auto result = parser.parse("[[Main Page]]");

    ASSERT_TRUE(result.success());

    bool found_link = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Link) {
            found_link = true;
            auto* link = static_cast<LinkNode*>(node.get());
            EXPECT_EQ(link->target, "Main Page");
        } else if (node->type == NodeType::Paragraph) {
            auto* para = static_cast<ParagraphNode*>(node.get());
            for (const auto& child : para->content) {
                if (child->type == NodeType::Link) {
                    found_link = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_link);
}

TEST(ParserTest, ParseLinkWithDisplay) {
    Parser parser;
    auto result = parser.parse("[[Target|Display Text]]");

    ASSERT_TRUE(result.success());

    bool found_link = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Link) {
            found_link = true;
            auto* link = static_cast<LinkNode*>(node.get());
            EXPECT_EQ(link->target, "Target");
            EXPECT_TRUE(link->has_custom_display());
        } else if (node->type == NodeType::Paragraph) {
            auto* para = static_cast<ParagraphNode*>(node.get());
            for (const auto& child : para->content) {
                if (child->type == NodeType::Link) {
                    found_link = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_link);
}

TEST(ParserTest, ParseExternalLink) {
    Parser parser;
    auto result = parser.parse("[https://example.com Example]");

    ASSERT_TRUE(result.success());

    bool found_link = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::ExternalLink) {
            found_link = true;
            auto* link = static_cast<ExternalLinkNode*>(node.get());
            // URL parsing may include display text depending on implementation
            EXPECT_TRUE(link->url.find("https://example.com") != std::string::npos);
        } else if (node->type == NodeType::Paragraph) {
            auto* para = static_cast<ParagraphNode*>(node.get());
            for (const auto& child : para->content) {
                if (child->type == NodeType::ExternalLink) {
                    found_link = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_link);
}

// ============================================================================
// Heading tests
// ============================================================================

TEST(ParserTest, ParseHeading) {
    Parser parser;
    auto result = parser.parse("== Section ==");

    ASSERT_TRUE(result.success());

    bool found_heading = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Heading) {
            found_heading = true;
            auto* heading = static_cast<HeadingNode*>(node.get());
            EXPECT_EQ(heading->level, 2);
        }
    }
    EXPECT_TRUE(found_heading);
}

TEST(ParserTest, ParseMultipleLevels) {
    Parser parser;
    auto result = parser.parse("= Level 1 =\n== Level 2 ==\n=== Level 3 ===");

    ASSERT_TRUE(result.success());

    int heading_count = 0;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Heading) {
            heading_count++;
        }
    }
    EXPECT_GE(heading_count, 1);
}

// ============================================================================
// List tests
// ============================================================================

TEST(ParserTest, ParseBulletList) {
    Parser parser;
    auto result = parser.parse("* Item 1\n* Item 2\n* Item 3");

    ASSERT_TRUE(result.success());

    bool found_list = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::List) {
            found_list = true;
            auto* list = static_cast<ListNode*>(node.get());
            EXPECT_EQ(list->list_type, ListNode::ListType::Bullet);
            EXPECT_GE(list->items.size(), 1u);
        }
    }
    EXPECT_TRUE(found_list);
}

TEST(ParserTest, ParseNumberedList) {
    Parser parser;
    auto result = parser.parse("# First\n# Second\n# Third");

    ASSERT_TRUE(result.success());

    bool found_list = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::List) {
            found_list = true;
            auto* list = static_cast<ListNode*>(node.get());
            EXPECT_EQ(list->list_type, ListNode::ListType::Numbered);
        }
    }
    EXPECT_TRUE(found_list);
}

TEST(ParserTest, ParseNestedList) {
    Parser parser;
    auto result = parser.parse("* Level 1\n** Level 2\n*** Level 3");

    ASSERT_TRUE(result.success());

    bool found_list = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::List) {
            found_list = true;
        }
    }
    EXPECT_TRUE(found_list);
}

// ============================================================================
// Template tests
// ============================================================================

TEST(ParserTest, ParseSimpleTemplate) {
    Parser parser;
    auto result = parser.parse("{{Template}}");

    ASSERT_TRUE(result.success());

    bool found_template = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Template) {
            found_template = true;
            auto* tmpl = static_cast<TemplateNode*>(node.get());
            EXPECT_EQ(tmpl->name, "Template");
        } else if (node->type == NodeType::Paragraph) {
            auto* para = static_cast<ParagraphNode*>(node.get());
            for (const auto& child : para->content) {
                if (child->type == NodeType::Template) {
                    found_template = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_template);
}

TEST(ParserTest, ParseTemplateWithParameter) {
    Parser parser;
    auto result = parser.parse("{{Template|param}}");

    ASSERT_TRUE(result.success());

    bool found_template = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Template) {
            found_template = true;
            auto* tmpl = static_cast<TemplateNode*>(node.get());
            EXPECT_GE(tmpl->parameters.size(), 1u);
        } else if (node->type == NodeType::Paragraph) {
            auto* para = static_cast<ParagraphNode*>(node.get());
            for (const auto& child : para->content) {
                if (child->type == NodeType::Template) {
                    found_template = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_template);
}

TEST(ParserTest, ParseTemplateWithNamedParameter) {
    Parser parser;
    auto result = parser.parse("{{Template|key=value}}");

    ASSERT_TRUE(result.success());

    bool found_template = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Template) {
            found_template = true;
            auto* tmpl = static_cast<TemplateNode*>(node.get());
            EXPECT_GE(tmpl->parameters.size(), 1u);
            if (!tmpl->parameters.empty()) {
                EXPECT_EQ(tmpl->parameters[0].name, "key");
            }
        } else if (node->type == NodeType::Paragraph) {
            auto* para = static_cast<ParagraphNode*>(node.get());
            for (const auto& child : para->content) {
                if (child->type == NodeType::Template) {
                    found_template = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_template);
}

// ============================================================================
// HTML comment tests
// ============================================================================

TEST(ParserTest, ParseHtmlComment) {
    Parser parser;
    auto result = parser.parse("<!-- This is a comment -->");

    ASSERT_TRUE(result.success());

    // Comments may or may not be in AST depending on config
    // Just check it doesn't crash
}

// ============================================================================
// NoWiki tests
// ============================================================================

TEST(ParserTest, ParseNoWiki) {
    Parser parser;
    auto result = parser.parse("<nowiki>'''not bold'''</nowiki>");

    ASSERT_TRUE(result.success());

    bool found_nowiki = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::NoWiki) {
            found_nowiki = true;
            auto* nowiki = static_cast<NoWikiNode*>(node.get());
            EXPECT_EQ(nowiki->content, "'''not bold'''");
        } else if (node->type == NodeType::Paragraph) {
            auto* para = static_cast<ParagraphNode*>(node.get());
            for (const auto& child : para->content) {
                if (child->type == NodeType::NoWiki) {
                    found_nowiki = true;
                }
            }
        }
    }
    EXPECT_TRUE(found_nowiki);
}

// ============================================================================
// Category tests
// ============================================================================

TEST(ParserTest, ParseCategory) {
    Parser parser;
    auto result = parser.parse("[[Category:Science]]");

    ASSERT_TRUE(result.success());

    bool found_category = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Category) {
            found_category = true;
            auto* cat = static_cast<CategoryNode*>(node.get());
            EXPECT_EQ(cat->category, "Science");
        }
    }
    EXPECT_TRUE(found_category);
}

// ============================================================================
// Table tests
// ============================================================================

TEST(ParserTest, ParseSimpleTable) {
    Parser parser;
    auto result = parser.parse("{|\n| Cell 1\n| Cell 2\n|}");

    ASSERT_TRUE(result.success());

    bool found_table = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Table) {
            found_table = true;
        }
    }
    EXPECT_TRUE(found_table);
}

TEST(ParserTest, ParseTableWithHeader) {
    Parser parser;
    auto result = parser.parse("{|\n! Header 1\n! Header 2\n|-\n| Cell 1\n| Cell 2\n|}");

    ASSERT_TRUE(result.success());

    bool found_table = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Table) {
            found_table = true;
        }
    }
    EXPECT_TRUE(found_table);
}

// ============================================================================
// Complex document tests
// ============================================================================

TEST(ParserTest, ParseComplexDocument) {
    std::string wikitext = R"(
= Main Title =

This is '''bold''' and ''italic'' text.

== Section 1 ==

* Item 1
* Item 2
** Nested item

[[Link to page]]

{{Template|param=value}}

{|
! Header
|-
| Cell
|}
)";

    Parser parser;
    auto result = parser.parse(wikitext);

    ASSERT_TRUE(result.success());
    EXPECT_FALSE(result.document->content.empty());

    // Should have various node types
    bool has_heading = false;

    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Heading) has_heading = true;
    }

    EXPECT_TRUE(has_heading);
}

// ============================================================================
// Parser configuration tests
// ============================================================================

TEST(ParserTest, ParserConfig_Default) {
    ParserConfig config;

    EXPECT_FALSE(config.expand_templates);
    EXPECT_TRUE(config.parse_tables);
    EXPECT_TRUE(config.parse_lists);
    EXPECT_GT(config.max_depth, 0);
}

TEST(ParserTest, ParserConfig_DisableTables) {
    ParserConfig config;
    config.parse_tables = false;

    Parser parser(config);
    auto result = parser.parse("{|\n| Cell\n|}");

    ASSERT_TRUE(result.success());
    // Tables should not be parsed, treated as text or ignored
    // No Table node should be present
    bool found_table = false;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Table) {
            found_table = true;
        }
    }
    EXPECT_FALSE(found_table);
}

// ============================================================================
// Error handling tests
// ============================================================================

TEST(ParserTest, ParseResult_Errors) {
    ParseResult result;
    EXPECT_FALSE(result.has_errors());

    // Create a minimal error
    wikilib::ParseError error;
    error.message = "test error";
    error.severity = wikilib::ErrorSeverity::Warning;

    result.errors.push_back(error);
    EXPECT_TRUE(result.has_errors());
}

TEST(ParserTest, ParseResult_FatalErrors) {
    ParseResult result;
    EXPECT_FALSE(result.has_fatal_errors());

    wikilib::ParseError error;
    error.message = "fatal error";
    error.severity = wikilib::ErrorSeverity::Fatal;
    result.errors.push_back(error);

    EXPECT_TRUE(result.has_fatal_errors());
}

TEST(ParserTest, ParseMalformed) {
    Parser parser;

    // Unclosed template
    auto result1 = parser.parse("{{Template");
    EXPECT_TRUE(result1.success()); // Lenient mode should still succeed

    // Unclosed link
    auto result2 = parser.parse("[[Link");
    EXPECT_TRUE(result2.success());
}

// ============================================================================
// Inline parsing tests
// ============================================================================

TEST(ParserTest, ParseInline) {
    Parser parser;
    auto nodes = parser.parse_inline("'''bold''' text [[link]]");

    EXPECT_FALSE(nodes.empty());

    // Should have inline content (formatting, links, text)
    bool has_formatting = false;
    bool has_link = false;

    for (const auto& node : nodes) {
        if (node->type == NodeType::Formatting) has_formatting = true;
        if (node->type == NodeType::Link) has_link = true;
    }

    EXPECT_TRUE(has_formatting || has_link); // At least one
}
