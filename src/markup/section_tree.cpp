/**
 * @file section_tree.cpp
 * @brief Implementation of section tree building
 */

#include "wikilib/markup/section_tree.h"
#include "wikilib/core/text_utils.hpp"
#include <sstream>
#include <stack>

namespace wikilib::markup {

// ============================================================================
// SectionNode methods
// ============================================================================

size_t SectionNode::total_content_count() const noexcept {
    size_t count = content.size();
    for (const auto& child : children) {
        count += child->total_content_count();
    }
    return count;
}

size_t SectionNode::total_section_count() const noexcept {
    size_t count = children.size();
    for (const auto& child : children) {
        count += child->total_section_count();
    }
    return count;
}

// ============================================================================
// Tree building
// ============================================================================

SectionNodePtr build_section_tree(const DocumentNode* doc) {
    if (!doc) {
        auto root = std::make_shared<SectionNode>();
        root->level = 0;
        return root;
    }
    return build_section_tree(doc->content);
}

SectionNodePtr build_section_tree(const std::vector<NodePtr>& nodes) {
    auto root = std::make_shared<SectionNode>();
    root->level = 0;

    // Stack: pair of (level, section node)
    std::stack<std::pair<int, SectionNodePtr>> stack;
    stack.emplace(0, root);

    for (const auto& node : nodes) {
        if (node->type == NodeType::Heading) {
            auto* heading = static_cast<HeadingNode*>(node.get());

            // Create new section
            auto section = std::make_shared<SectionNode>();
            section->level = heading->level;

            // Extract title from heading content
            for (const auto& child : heading->content) {
                if (child->type == NodeType::Text) {
                    auto* text = static_cast<TextNode*>(child.get());
                    section->title += text->text;
                }
            }

            // Trim whitespace from title
            section->title = std::string(wikilib::text::trim(section->title));

            // Find parent with lower level
            while (!stack.empty() && stack.top().first >= heading->level) {
                stack.pop();
            }

            // Add as child of parent
            stack.top().second->children.push_back(section);
            stack.emplace(heading->level, section);
        } else {
            // Add content to current section
            stack.top().second->content.push_back(node.get());
        }
    }

    return root;
}

// ============================================================================
// Tree printing
// ============================================================================

void print_section_tree(
    const SectionNodePtr& node,
    std::ostream& out,
    const std::string& prefix,
    bool is_last
) {
    if (!node) {
        return;
    }

    // Print current node
    if (!node->is_root()) {
        out << prefix;
        out << (is_last ? "└── " : "├── ");
        out << node->title << "(" << node->content.size() << ")\n";
    } else {
        // Root node
        out << "ROOT(" << node->content.size() << ")\n";
    }

    // Print children
    for (size_t i = 0; i < node->children.size(); ++i) {
        bool last = (i == node->children.size() - 1);
        std::string new_prefix = prefix;
        if (!node->is_root()) {
            new_prefix += (is_last ? "    " : "│   ");
        }
        print_section_tree(node->children[i], out, new_prefix, last);
    }
}

std::string section_tree_to_string(const SectionNodePtr& node) {
    std::ostringstream oss;
    print_section_tree(node, oss);
    return oss.str();
}

// ============================================================================
// Tree traversal
// ============================================================================

void traverse_sections(
    const SectionNodePtr& node,
    const std::function<void(const SectionNodePtr&)>& visitor
) {
    if (!node) {
        return;
    }

    visitor(node);

    for (const auto& child : node->children) {
        traverse_sections(child, visitor);
    }
}

SectionNodePtr find_section(
    const SectionNodePtr& node,
    std::string_view title
) {
    if (!node) {
        return nullptr;
    }

    if (node->title == title) {
        return node;
    }

    for (const auto& child : node->children) {
        auto found = find_section(child, title);
        if (found) {
            return found;
        }
    }

    return nullptr;
}

} // namespace wikilib::markup
