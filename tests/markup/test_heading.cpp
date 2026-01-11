/**
 * @file test_heading.cpp
 * @brief Tests for MediaWiki heading/header parsing
 *
 * Based on HeaderTest.cpp and test_header_parsing.cpp from z_innego_projektu
 */

#include <gtest/gtest.h>
#include "wikilib/markup/parser.h"
#include "wikilib/markup/tokenizer.h"

using namespace wikilib;
using namespace wikilib::markup;

class HeadingTest : public ::testing::Test {
protected:
    // Helper to parse and extract heading info
    struct HeadingInfo {
        int level = 0;
        std::string title;
        bool found = false;
    };

    HeadingInfo parseHeading(const std::string& input) {
        Parser parser;
        auto result = parser.parse(input);

        HeadingInfo info;
        if (!result.success()) {
            return info;
        }

        for (const auto& node : result.document->content) {
            if (node->type == NodeType::Heading) {
                info.found = true;
                auto* heading = static_cast<HeadingNode*>(node.get());
                info.level = heading->level;

                // Extract title from content
                for (const auto& child : heading->content) {
                    if (child->type == NodeType::Text) {
                        auto* text = static_cast<TextNode*>(child.get());
                        info.title += text->text;
                    }
                }
                break;
            }
        }

        return info;
    }
};

// ============================================================================
// Basic heading tests
// ============================================================================

TEST_F(HeadingTest, SingleEquals) {
    // =header= should work with level 1
    auto info = parseHeading("=header=");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(1, info.level);
    EXPECT_EQ("header", info.title);
}

TEST_F(HeadingTest, DoubleEquals) {
    auto info = parseHeading("==header==");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(2, info.level);
    EXPECT_EQ("header", info.title);
}

TEST_F(HeadingTest, TripleEquals) {
    auto info = parseHeading("===123===");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(3, info.level);
    EXPECT_EQ("123", info.title);
}

TEST_F(HeadingTest, MaxLevel6) {
    auto info = parseHeading("======header======");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(6, info.level);
    EXPECT_EQ("header", info.title);
}

// ============================================================================
// Unbalanced equals signs
// ============================================================================

TEST_F(HeadingTest, UnbalancedMore_Before) {
    // ===header= should use min(3,1)=1, title="==header"
    auto info = parseHeading("===header=");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(1, info.level);
    EXPECT_EQ("==header", info.title);
}

TEST_F(HeadingTest, UnbalancedMore_After) {
    // =header=== should use min(1,3)=1, title="header=="
    auto info = parseHeading("=header===");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(1, info.level);
    EXPECT_EQ("header==", info.title);
}

TEST_F(HeadingTest, UnbalancedLevel2) {
    // ===abc== should use min(3,2)=2, title="=abc"
    auto info = parseHeading("===abc==");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(2, info.level);
    EXPECT_EQ("=abc", info.title);
}

// ============================================================================
// Whitespace handling
// ============================================================================

TEST_F(HeadingTest, TrailingSpaces_Valid) {
    // ==header==   (trailing spaces OK)
    auto info = parseHeading("==header==  ");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(2, info.level);
    EXPECT_EQ("header", info.title);
}

TEST_F(HeadingTest, TrailingText_Invalid) {
    // ==header==  abc (text after closing = invalid)
    auto info = parseHeading("==header==  abc");
    EXPECT_FALSE(info.found);
}

TEST_F(HeadingTest, LeadingSpaces_Invalid) {
    // Leading spaces make it invalid (not at line start)
    auto info = parseHeading(" ==header==");
    EXPECT_FALSE(info.found);
}

TEST_F(HeadingTest, LeadingSpaces_MultipleInvalid) {
    auto info = parseHeading("    ==header==");
    EXPECT_FALSE(info.found);
}

// ============================================================================
// Equals inside heading
// ============================================================================

TEST_F(HeadingTest, EqualsInside) {
    // =header==header= should give title "header==header"
    auto info = parseHeading("=header==header=");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(1, info.level);
    EXPECT_EQ("header==header", info.title);
}

// ============================================================================
// Comment handling
// ============================================================================

TEST_F(HeadingTest, CommentInside) {
    // == <!-- dd --> aa== should give title "aa"
    // Comments are filtered out
    auto info = parseHeading("== <!-- dd --> aa==");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(2, info.level);
    EXPECT_EQ(" aa", info.title); // Space before "aa" remains
}

TEST_F(HeadingTest, CommentBefore) {
    // <!-- dd -->==aa== is valid (comment before heading)
    auto info = parseHeading("<!-- dd -->==aa==");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(2, info.level);
    EXPECT_EQ("aa", info.title);
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_F(HeadingTest, AloneEquals_Empty) {
    // Empty string
    auto info = parseHeading("");
    EXPECT_FALSE(info.found);
}

TEST_F(HeadingTest, AloneEquals_Single) {
    // Single =
    auto info = parseHeading("=");
    EXPECT_FALSE(info.found);
}

TEST_F(HeadingTest, AloneEquals_Double) {
    // ==
    auto info = parseHeading("==");
    EXPECT_FALSE(info.found);
}

TEST_F(HeadingTest, AloneEquals_Triple) {
    // === should give level 1, title "="
    auto info = parseHeading("===");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(1, info.level);
    EXPECT_EQ("=", info.title);
}

TEST_F(HeadingTest, AloneEquals_Four) {
    // ==== should give level 2, title ""
    auto info = parseHeading("====");
    EXPECT_TRUE(info.found);
    EXPECT_EQ(2, info.level);
    EXPECT_EQ("", info.title);
}

// ============================================================================
// Multiline tests
// ============================================================================

TEST_F(HeadingTest, MultiLine_AllValid) {
    std::string input = "=header 1=\n==header 2==\n===header 3===\n====header 4====\n";
    Parser parser;
    auto result = parser.parse(input);

    ASSERT_TRUE(result.success());

    std::vector<std::string> expected_titles = {"header 1", "header 2", "header 3", "header 4"};
    std::vector<int> expected_levels = {1, 2, 3, 4};

    int heading_count = 0;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Heading) {
            auto* heading = static_cast<HeadingNode*>(node.get());
            ASSERT_LT(heading_count, expected_titles.size());

            EXPECT_EQ(expected_levels[heading_count], heading->level);

            std::string title;
            for (const auto& child : heading->content) {
                if (child->type == NodeType::Text) {
                    title += static_cast<TextNode*>(child.get())->text;
                }
            }
            EXPECT_EQ(expected_titles[heading_count], title);

            heading_count++;
        }
    }

    EXPECT_EQ(4, heading_count);
}

TEST_F(HeadingTest, MultiLine_SomeInvalid) {
    // Lines with leading spaces should be invalid
    std::string input = "=header 1=\n ==header 2==\n===header 3===\n ===header 4====\n";
    Parser parser;
    auto result = parser.parse(input);

    ASSERT_TRUE(result.success());

    std::vector<std::string> expected_titles = {"header 1", "header 3"};
    std::vector<int> expected_levels = {1, 3};

    int heading_count = 0;
    for (const auto& node : result.document->content) {
        if (node->type == NodeType::Heading) {
            auto* heading = static_cast<HeadingNode*>(node.get());
            ASSERT_LT(heading_count, expected_titles.size());

            EXPECT_EQ(expected_levels[heading_count], heading->level);

            std::string title;
            for (const auto& child : heading->content) {
                if (child->type == NodeType::Text) {
                    title += static_cast<TextNode*>(child.get())->text;
                }
            }
            EXPECT_EQ(expected_titles[heading_count], title);

            heading_count++;
        }
    }

    EXPECT_EQ(2, heading_count); // Only 2 valid headings
}
