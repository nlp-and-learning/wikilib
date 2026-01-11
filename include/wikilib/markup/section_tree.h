#pragma once

/**
 * @file section_tree.h
 * @brief Section tree builder for document hierarchy
 */

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "wikilib/markup/ast.h"

namespace wikilib::markup {

// ============================================================================
// Section tree node
// ============================================================================

/**
 * @brief Node in section tree representing document hierarchy
 */
struct SectionNode {
    std::string title;      // Section title (empty for root)
    int level = 0;          // Heading level (0 for root, 1-6 for sections)

    std::vector<Node*> content;  // Content of this section (before subsections)
    std::vector<std::shared_ptr<SectionNode>> children;  // Subsections

    /**
     * @brief Check if this is the root node
     */
    [[nodiscard]] bool is_root() const noexcept {
        return level == 0 && title.empty();
    }

    /**
     * @brief Get total number of content nodes (including all descendants)
     */
    [[nodiscard]] size_t total_content_count() const noexcept;

    /**
     * @brief Get total number of sections (including all descendants)
     */
    [[nodiscard]] size_t total_section_count() const noexcept;
};

using SectionNodePtr = std::shared_ptr<SectionNode>;

// ============================================================================
// Tree building
// ============================================================================

/**
 * @brief Build section tree from document AST
 *
 * Creates hierarchical structure based on heading levels.
 * Content between headings is attached to the appropriate section.
 *
 * Example:
 *   == Section 1 ==
 *   Text 1
 *   === Subsection 1.1 ===
 *   Text 1.1
 *   == Section 2 ==
 *   Text 2
 *
 * Becomes:
 *   ROOT
 *   ├── Section 1 (level=2)
 *   │   ├── content: [Text 1]
 *   │   └── Subsection 1.1 (level=3)
 *   │       └── content: [Text 1.1]
 *   └── Section 2 (level=2)
 *       └── content: [Text 2]
 *
 * @param doc Document to build tree from
 * @return Root node of section tree
 */
[[nodiscard]] SectionNodePtr build_section_tree(const DocumentNode* doc);

/**
 * @brief Build section tree from list of nodes
 *
 * @param nodes List of AST nodes
 * @return Root node of section tree
 */
[[nodiscard]] SectionNodePtr build_section_tree(const std::vector<NodePtr>& nodes);

// ============================================================================
// Tree printing
// ============================================================================

/**
 * @brief Print section tree in ASCII format
 *
 * Example output:
 *   ROOT(2)
 *   ├── Introduction(1)
 *   │   └── Background(0)
 *   └── Methods(3)
 *       ├── Setup(1)
 *       └── Analysis(2)
 *
 * Numbers in parentheses show content node count.
 *
 * @param node Section tree root
 * @param out Output stream
 * @param prefix Current line prefix (used for recursion)
 * @param is_last Whether this is the last child
 */
void print_section_tree(
    const SectionNodePtr& node,
    std::ostream& out,
    const std::string& prefix = "",
    bool is_last = true
);

/**
 * @brief Get section tree as string
 *
 * @param node Section tree root
 * @return Tree representation as string
 */
[[nodiscard]] std::string section_tree_to_string(const SectionNodePtr& node);

// ============================================================================
// Tree traversal
// ============================================================================

/**
 * @brief Visit all sections in tree (depth-first)
 *
 * @param node Section tree root
 * @param visitor Function called for each section
 */
void traverse_sections(
    const SectionNodePtr& node,
    const std::function<void(const SectionNodePtr&)>& visitor
);

/**
 * @brief Find section by title (case-sensitive)
 *
 * @param node Section tree root
 * @param title Section title to find
 * @return Section node or nullptr if not found
 */
[[nodiscard]] SectionNodePtr find_section(
    const SectionNodePtr& node,
    std::string_view title
);

} // namespace wikilib::markup
