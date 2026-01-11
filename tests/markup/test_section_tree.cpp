/**
 * @file test_section_tree.cpp
 * @brief Tests for section tree building from AST
 */

#include <gtest/gtest.h>
#include "wikilib/markup/parser.h"
#include "wikilib/markup/section_tree.h"

using namespace wikilib::markup;

// ============================================================================
// Basic section tree tests
// ============================================================================

TEST(SectionTreeTest, EmptyDocument) {
    auto doc = parse("");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    EXPECT_TRUE(tree->title.empty());
    EXPECT_EQ(tree->level, 0);
    EXPECT_TRUE(tree->is_root());
    EXPECT_EQ(tree->content.size(), 0u);
    EXPECT_EQ(tree->children.size(), 0u);
}

TEST(SectionTreeTest, SingleTextNode) {
    auto doc = parse("Hello world");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    EXPECT_TRUE(tree->title.empty());
    EXPECT_EQ(tree->level, 0);
    EXPECT_EQ(tree->content.size(), 1u);
    EXPECT_EQ(tree->children.size(), 0u);
}

TEST(SectionTreeTest, SingleSection) {
    auto doc = parse("== Introduction ==\nSome text here.");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    // Root should have no content, one child
    EXPECT_EQ(tree->content.size(), 0u);
    EXPECT_EQ(tree->children.size(), 1u);

    // Check first section
    auto& section = tree->children[0];
    EXPECT_EQ(section->title, "Introduction");
    EXPECT_EQ(section->level, 2);
    EXPECT_FALSE(section->is_root());
    EXPECT_EQ(section->content.size(), 1u);  // "Some text here."
    EXPECT_EQ(section->children.size(), 0u);
}

TEST(SectionTreeTest, TwoSectionsAtSameLevel) {
    auto doc = parse(R"(
== Section 1 ==
Text 1
== Section 2 ==
Text 2
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    // Root should have 2 children at same level
    EXPECT_EQ(tree->children.size(), 2u);

    EXPECT_EQ(tree->children[0]->title, "Section 1");
    EXPECT_EQ(tree->children[0]->level, 2);
    EXPECT_GT(tree->children[0]->content.size(), 0u);

    EXPECT_EQ(tree->children[1]->title, "Section 2");
    EXPECT_EQ(tree->children[1]->level, 2);
    EXPECT_GT(tree->children[1]->content.size(), 0u);
}

// ============================================================================
// Nested section tests
// ============================================================================

TEST(SectionTreeTest, NestedSections) {
    auto doc = parse(R"(
== Section 1 ==
Text 1
=== Subsection 1.1 ===
Text 1.1
== Section 2 ==
Text 2
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    // Root should have 2 top-level sections
    EXPECT_EQ(tree->children.size(), 2u);

    // Section 1 should have a subsection
    auto& sec1 = tree->children[0];
    EXPECT_EQ(sec1->title, "Section 1");
    EXPECT_EQ(sec1->level, 2);
    EXPECT_EQ(sec1->children.size(), 1u);

    // Check subsection
    auto& subsec = sec1->children[0];
    EXPECT_EQ(subsec->title, "Subsection 1.1");
    EXPECT_EQ(subsec->level, 3);
    EXPECT_GT(subsec->content.size(), 0u);
    EXPECT_EQ(subsec->children.size(), 0u);

    // Section 2 should have no subsections
    auto& sec2 = tree->children[1];
    EXPECT_EQ(sec2->title, "Section 2");
    EXPECT_EQ(sec2->level, 2);
    EXPECT_EQ(sec2->children.size(), 0u);
}

TEST(SectionTreeTest, DeepNesting) {
    auto doc = parse(R"(
== Level 2 ==
Text at level 2
=== Level 3 ===
Text at level 3
==== Level 4 ====
Text at level 4
===== Level 5 =====
Text at level 5
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    // Check nesting depth
    EXPECT_EQ(tree->children.size(), 1u);

    auto* current = tree->children[0].get();
    EXPECT_EQ(current->level, 2);
    EXPECT_EQ(current->children.size(), 1u);

    current = current->children[0].get();
    EXPECT_EQ(current->level, 3);
    EXPECT_EQ(current->children.size(), 1u);

    current = current->children[0].get();
    EXPECT_EQ(current->level, 4);
    EXPECT_EQ(current->children.size(), 1u);

    current = current->children[0].get();
    EXPECT_EQ(current->level, 5);
    EXPECT_EQ(current->children.size(), 0u);
}

TEST(SectionTreeTest, SkippedLevels) {
    auto doc = parse(R"(
== Level 2 ==
Text
==== Level 4 ====
Skipped level 3
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    // Level 4 should still be child of level 2
    EXPECT_EQ(tree->children.size(), 1u);
    auto& sec2 = tree->children[0];
    EXPECT_EQ(sec2->level, 2);
    EXPECT_EQ(sec2->children.size(), 1u);

    auto& sec4 = sec2->children[0];
    EXPECT_EQ(sec4->level, 4);
}

TEST(SectionTreeTest, LevelReduction) {
    auto doc = parse(R"(
== Level 2 ==
Text 2
=== Level 3 ===
Text 3
==== Level 4 ====
Text 4
=== Back to Level 3 ===
Text 3 again
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    auto& sec2 = tree->children[0];
    EXPECT_EQ(sec2->children.size(), 2u);  // Two level 3 sections

    auto& sec3_first = sec2->children[0];
    EXPECT_EQ(sec3_first->level, 3);
    EXPECT_EQ(sec3_first->children.size(), 1u);  // Has level 4 child

    auto& sec3_second = sec2->children[1];
    EXPECT_EQ(sec3_second->level, 3);
    EXPECT_EQ(sec3_second->children.size(), 0u);  // No children
}

// ============================================================================
// Content placement tests
// ============================================================================

TEST(SectionTreeTest, ContentBeforeFirstHeading) {
    auto doc = parse(R"(
This is preamble text.
More preamble.
== First Section ==
Section text.
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    // Root should have preamble content
    EXPECT_GT(tree->content.size(), 0u);

    // And one child section
    EXPECT_EQ(tree->children.size(), 1u);
    EXPECT_EQ(tree->children[0]->title, "First Section");
}

TEST(SectionTreeTest, ContentBetweenHeadings) {
    auto doc = parse(R"(
== Section 1 ==
Content of section 1.
More content.
== Section 2 ==
Content of section 2.
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    EXPECT_EQ(tree->children.size(), 2u);

    // Each section should have its own content
    EXPECT_GT(tree->children[0]->content.size(), 0u);
    EXPECT_GT(tree->children[1]->content.size(), 0u);
}

TEST(SectionTreeTest, EmptySections) {
    auto doc = parse(R"(
== Section 1 ==
== Section 2 ==
Some content here.
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    EXPECT_EQ(tree->children.size(), 2u);

    // Section 1 has no content
    EXPECT_EQ(tree->children[0]->content.size(), 0u);

    // Section 2 has content
    EXPECT_GT(tree->children[1]->content.size(), 0u);
}

// ============================================================================
// Utility function tests
// ============================================================================

TEST(SectionTreeTest, TotalContentCount) {
    auto doc = parse(R"(
Preamble
== Section 1 ==
Text 1
=== Subsection 1.1 ===
Text 1.1
== Section 2 ==
Text 2
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    // Total content count includes all content in tree
    size_t total = tree->total_content_count();
    EXPECT_GT(total, 0u);
}

TEST(SectionTreeTest, TotalSectionCount) {
    auto doc = parse(R"(
== Section 1 ==
=== Subsection 1.1 ===
==== Subsubsection 1.1.1 ====
== Section 2 ==
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    // Should count all sections and subsections
    // 2 at level 2, 1 at level 3, 1 at level 4 = 4 total
    EXPECT_EQ(tree->total_section_count(), 4u);
}

TEST(SectionTreeTest, IsRootCheck) {
    auto doc = parse("== Section ==");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    EXPECT_TRUE(tree->is_root());
    EXPECT_FALSE(tree->children[0]->is_root());
}

// ============================================================================
// Tree traversal tests
// ============================================================================

TEST(SectionTreeTest, TraverseSections) {
    auto doc = parse(R"(
== Section 1 ==
=== Subsection 1.1 ===
== Section 2 ==
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    // Count sections using traversal
    int count = 0;
    traverse_sections(tree, [&count](const SectionNodePtr& node) {
        count++;
    });

    // Should visit: ROOT + Section 1 + Subsection 1.1 + Section 2 = 4
    EXPECT_EQ(count, 4);
}

TEST(SectionTreeTest, FindSectionByTitle) {
    auto doc = parse(R"(
== Introduction ==
== Methods ==
=== Setup ===
=== Analysis ===
== Results ==
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    // Find existing section
    auto methods = find_section(tree, "Methods");
    ASSERT_TRUE(methods != nullptr);
    EXPECT_EQ(methods->level, 2);
    EXPECT_EQ(methods->children.size(), 2u);

    // Find nested section
    auto setup = find_section(tree, "Setup");
    ASSERT_TRUE(setup != nullptr);
    EXPECT_EQ(setup->level, 3);

    // Find non-existent section
    auto missing = find_section(tree, "Nonexistent");
    EXPECT_TRUE(missing == nullptr);
}

// ============================================================================
// Tree printing tests
// ============================================================================

TEST(SectionTreeTest, PrintSimpleTree) {
    auto doc = parse(R"(
== Section 1 ==
Text
== Section 2 ==
More text
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    std::string output = section_tree_to_string(tree);

    // Should contain ROOT and both sections
    EXPECT_TRUE(output.find("ROOT") != std::string::npos);
    EXPECT_TRUE(output.find("Section 1") != std::string::npos);
    EXPECT_TRUE(output.find("Section 2") != std::string::npos);
}

TEST(SectionTreeTest, PrintNestedTree) {
    auto doc = parse(R"(
== Parent ==
=== Child 1 ===
=== Child 2 ===
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    std::string output = section_tree_to_string(tree);

    // Check tree structure indicators
    EXPECT_TRUE(output.find("Parent") != std::string::npos);
    EXPECT_TRUE(output.find("Child 1") != std::string::npos);
    EXPECT_TRUE(output.find("Child 2") != std::string::npos);
}

// ============================================================================
// Edge cases
// ============================================================================

TEST(SectionTreeTest, NullDocument) {
    auto tree = build_section_tree(nullptr);
    ASSERT_TRUE(tree != nullptr);

    EXPECT_TRUE(tree->title.empty());
    EXPECT_TRUE(tree->is_root());
    EXPECT_EQ(tree->content.size(), 0u);
    EXPECT_EQ(tree->children.size(), 0u);
}

TEST(SectionTreeTest, HeadingWithComplexContent) {
    auto doc = parse("== Section with ''italic'' and '''bold''' ==\nText");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    EXPECT_EQ(tree->children.size(), 1u);
    auto& section = tree->children[0];

    // Title should contain text from heading (may include markup)
    EXPECT_FALSE(section->title.empty());
}

TEST(SectionTreeTest, MultipleLevel1Headings) {
    auto doc = parse(R"(
= Heading 1 =
Content 1
= Heading 2 =
Content 2
)");
    ASSERT_TRUE(doc.success());

    auto tree = build_section_tree(doc.document.get());
    ASSERT_TRUE(tree != nullptr);

    // Both level 1 headings should be children of root
    EXPECT_EQ(tree->children.size(), 2u);
    EXPECT_EQ(tree->children[0]->level, 1);
    EXPECT_EQ(tree->children[1]->level, 1);
}
