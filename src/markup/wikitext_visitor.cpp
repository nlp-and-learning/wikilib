/**
 * @file wikitext_visitor.cpp
 * @brief Implementation of visitor pattern for wikitext AST
 */

#include "wikilib/markup/wikitext_visitor.h"
#include <algorithm>

namespace wikilib::markup {

// ============================================================================
// WikitextVisitor implementation
// ============================================================================

void WikitextVisitor::visit(Node &node) {
    switch (node.type) {
        case NodeType::Text:
            visit(static_cast<TextNode &>(node));
            break;
        case NodeType::Formatting:
            visit(static_cast<FormattingNode &>(node));
            break;
        case NodeType::Link:
            visit(static_cast<LinkNode &>(node));
            break;
        case NodeType::ExternalLink:
            visit(static_cast<ExternalLinkNode &>(node));
            break;
        case NodeType::Template:
            visit(static_cast<TemplateNode &>(node));
            break;
        case NodeType::Parameter:
            visit(static_cast<ParameterNode &>(node));
            break;
        case NodeType::Heading:
            visit(static_cast<HeadingNode &>(node));
            break;
        case NodeType::List:
            visit(static_cast<ListNode &>(node));
            break;
        case NodeType::ListItem:
            visit(static_cast<ListItemNode &>(node));
            break;
        case NodeType::Table:
            visit(static_cast<TableNode &>(node));
            break;
        case NodeType::TableRow:
            visit(static_cast<TableRowNode &>(node));
            break;
        case NodeType::TableCell:
            visit(static_cast<TableCellNode &>(node));
            break;
        case NodeType::HtmlTag:
            visit(static_cast<HtmlTagNode &>(node));
            break;
        case NodeType::Paragraph:
            visit(static_cast<ParagraphNode &>(node));
            break;
        case NodeType::Comment:
            visit(static_cast<CommentNode &>(node));
            break;
        case NodeType::NoWiki:
            visit(static_cast<NoWikiNode &>(node));
            break;
        case NodeType::Redirect:
            visit(static_cast<RedirectNode &>(node));
            break;
        case NodeType::Category:
            visit(static_cast<CategoryNode &>(node));
            break;
        case NodeType::MagicWord:
            visit(static_cast<MagicWordNode &>(node));
            break;
        case NodeType::HorizontalRule:
            visit(static_cast<HorizontalRuleNode &>(node));
            break;
        case NodeType::Document:
            visit(static_cast<DocumentNode &>(node));
            break;
    }
}

void WikitextVisitor::visit_children(Node &node) {
    for (auto &child: node.children()) {
        if (child) {
            visit(*child);
        }
    }
}

void WikitextVisitor::visit(TextNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(FormattingNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(LinkNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(ExternalLinkNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(TemplateNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(ParameterNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(HeadingNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(ListNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(ListItemNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(TableNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(TableRowNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(TableCellNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(HtmlTagNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(ParagraphNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(CommentNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(NoWikiNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(RedirectNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(CategoryNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(MagicWordNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(HorizontalRuleNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void WikitextVisitor::visit(DocumentNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

// ============================================================================
// ConstWikitextVisitor implementation
// ============================================================================

void ConstWikitextVisitor::visit(const Node &node) {
    switch (node.type) {
        case NodeType::Text:
            visit(static_cast<const TextNode &>(node));
            break;
        case NodeType::Formatting:
            visit(static_cast<const FormattingNode &>(node));
            break;
        case NodeType::Link:
            visit(static_cast<const LinkNode &>(node));
            break;
        case NodeType::ExternalLink:
            visit(static_cast<const ExternalLinkNode &>(node));
            break;
        case NodeType::Template:
            visit(static_cast<const TemplateNode &>(node));
            break;
        case NodeType::Parameter:
            visit(static_cast<const ParameterNode &>(node));
            break;
        case NodeType::Heading:
            visit(static_cast<const HeadingNode &>(node));
            break;
        case NodeType::List:
            visit(static_cast<const ListNode &>(node));
            break;
        case NodeType::ListItem:
            visit(static_cast<const ListItemNode &>(node));
            break;
        case NodeType::Table:
            visit(static_cast<const TableNode &>(node));
            break;
        case NodeType::TableRow:
            visit(static_cast<const TableRowNode &>(node));
            break;
        case NodeType::TableCell:
            visit(static_cast<const TableCellNode &>(node));
            break;
        case NodeType::HtmlTag:
            visit(static_cast<const HtmlTagNode &>(node));
            break;
        case NodeType::Paragraph:
            visit(static_cast<const ParagraphNode &>(node));
            break;
        case NodeType::Comment:
            visit(static_cast<const CommentNode &>(node));
            break;
        case NodeType::NoWiki:
            visit(static_cast<const NoWikiNode &>(node));
            break;
        case NodeType::Redirect:
            visit(static_cast<const RedirectNode &>(node));
            break;
        case NodeType::Category:
            visit(static_cast<const CategoryNode &>(node));
            break;
        case NodeType::MagicWord:
            visit(static_cast<const MagicWordNode &>(node));
            break;
        case NodeType::HorizontalRule:
            visit(static_cast<const HorizontalRuleNode &>(node));
            break;
        case NodeType::Document:
            visit(static_cast<const DocumentNode &>(node));
            break;
    }
}

void ConstWikitextVisitor::visit_children(const Node &node) {
    for (const auto &child: node.children()) {
        if (child) {
            visit(*child);
        }
    }
}

void ConstWikitextVisitor::visit(const TextNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const FormattingNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const LinkNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const ExternalLinkNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const TemplateNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const ParameterNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const HeadingNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const ListNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const ListItemNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const TableNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const TableRowNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const TableCellNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const HtmlTagNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const ParagraphNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const CommentNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const NoWikiNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const RedirectNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const CategoryNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const MagicWordNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const HorizontalRuleNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

void ConstWikitextVisitor::visit(const DocumentNode &node) {
    if (enter(node)) {
        visit_children(node);
        leave(node);
    }
}

// ============================================================================
// Functional visitors
// ============================================================================

bool walk(Node &root, NodeCallback callback) {
    if (!callback(root)) {
        return false;
    }
    for (auto &child: root.children()) {
        if (child && !walk(*child, callback)) {
            return false;
        }
    }
    return true;
}

bool walk(const Node &root, ConstNodeCallback callback) {
    if (!callback(root)) {
        return false;
    }
    for (const auto &child: root.children()) {
        if (child && !walk(static_cast<const Node &>(*child), callback)) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// LinkExtractor implementation
// ============================================================================

void LinkExtractor::visit(const LinkNode &node) {
    ExtractedLink link;
    link.target = node.full_target();
    link.is_external = false;
    link.location = node.location;

    if (node.has_custom_display()) {
        for (const auto &child: node.display_content) {
            link.display += child->to_plain_text();
        }
    } else {
        link.display = node.target;
    }

    links_.push_back(std::move(link));

    // Continue visiting children
    visit_children(node);
}

void LinkExtractor::visit(const ExternalLinkNode &node) {
    ExtractedLink link;
    link.target = node.url;
    link.is_external = true;
    link.location = node.location;

    if (!node.display_content.empty()) {
        for (const auto &child: node.display_content) {
            link.display += child->to_plain_text();
        }
    } else {
        link.display = node.url;
    }

    links_.push_back(std::move(link));

    // Continue visiting children
    visit_children(node);
}

// ============================================================================
// TemplateExtractor implementation
// ============================================================================

void TemplateExtractor::visit(const TemplateNode &node) {
    ExtractedTemplate tmpl;
    tmpl.name = node.name;
    tmpl.location = node.location;

    for (const auto &param: node.parameters) {
        std::string name = param.name.value_or("");
        std::string value;
        for (const auto &val_node: param.value) {
            value += val_node->to_plain_text();
        }
        tmpl.parameters.emplace_back(std::move(name), std::move(value));
    }

    templates_.push_back(std::move(tmpl));

    // Visit template parameter values for nested templates
    for (const auto &param: node.parameters) {
        for (const auto &val_node: param.value) {
            if (val_node) {
                ConstWikitextVisitor::visit(*val_node);
            }
        }
    }
}

// ============================================================================
// CategoryExtractor implementation
// ============================================================================

void CategoryExtractor::visit(const CategoryNode &node) {
    ExtractedCategory cat;
    cat.name = node.category;
    cat.sort_key = node.sort_key;
    categories_.push_back(std::move(cat));

    visit_children(node);
}

// ============================================================================
// SectionExtractor implementation
// ============================================================================

void SectionExtractor::visit(const HeadingNode &node) {
    Section section;
    section.level = node.level;
    section.location = node.location;

    // Extract title text
    for (const auto &child: node.content) {
        section.title += child->to_plain_text();
    }

    // Find where to insert this section
    // Pop sections from stack until we find a parent with lower level
    while (!section_stack_.empty() && section_stack_.back()->level >= section.level) {
        section_stack_.pop_back();
    }

    if (section_stack_.empty()) {
        // Top-level section
        root_sections_.push_back(std::move(section));
        section_stack_.push_back(&root_sections_.back());
    } else {
        // Subsection
        section_stack_.back()->subsections.push_back(std::move(section));
        section_stack_.push_back(&section_stack_.back()->subsections.back());
    }

    visit_children(node);
}

// ============================================================================
// PlainTextExtractor implementation
// ============================================================================

void PlainTextExtractor::visit(const TextNode &node) {
    if (add_space_ && !text_.empty() && !node.text.empty()) {
        char last = text_.back();
        char first = node.text.front();
        if (last != ' ' && last != '\n' && first != ' ' && first != '\n') {
            text_ += ' ';
        }
    }
    text_ += node.text;
    add_space_ = false;
}

void PlainTextExtractor::visit(const NoWikiNode &node) {
    text_ += node.content;
}

bool PlainTextExtractor::enter(const Node &node) {
    // Add spacing around certain block elements
    switch (node.type) {
        case NodeType::Paragraph:
        case NodeType::Heading:
        case NodeType::ListItem:
        case NodeType::TableCell:
            add_space_ = true;
            break;
        default:
            break;
    }
    return true;
}

void PlainTextExtractor::leave(const Node &node) {
    // Add newline after block elements
    switch (node.type) {
        case NodeType::Paragraph:
        case NodeType::Heading:
            if (!text_.empty() && text_.back() != '\n') {
                text_ += '\n';
            }
            break;
        case NodeType::ListItem:
            if (!text_.empty() && text_.back() != '\n') {
                text_ += '\n';
            }
            break;
        default:
            break;
    }
}

// ============================================================================
// TemplateRemover implementation
// ============================================================================

bool TemplateRemover::enter(Node &node) {
    // Remove template nodes from parent's children
    auto &children = node.children();
    children.erase(std::remove_if(children.begin(), children.end(),
                                  [](const NodePtr &child) { return child && child->type == NodeType::Template; }),
                   children.end());

    return true;
}

// ============================================================================
// FormattingStripper implementation
// ============================================================================

void FormattingStripper::visit(FormattingNode &node) {
    // Move formatting node's content to its parent
    // This is a simplified implementation - full implementation would
    // need to properly splice children into parent's child list

    // For now, just visit children
    visit_children(node);
}

} // namespace wikilib::markup

