/**
 * @file ast.cpp
 * @brief Implementation of AST nodes for MediaWiki wikitext
 */

#include "wikilib/markup/ast.h"
#include <sstream>

namespace wikilib::markup {

// ============================================================================
// Node type names
// ============================================================================

std::string_view node_type_name(NodeType type) noexcept {
    switch (type) {
        case NodeType::Text:
            return "Text";
        case NodeType::Formatting:
            return "Formatting";
        case NodeType::Link:
            return "Link";
        case NodeType::ExternalLink:
            return "ExternalLink";
        case NodeType::Template:
            return "Template";
        case NodeType::Parameter:
            return "Parameter";
        case NodeType::Heading:
            return "Heading";
        case NodeType::List:
            return "List";
        case NodeType::ListItem:
            return "ListItem";
        case NodeType::Table:
            return "Table";
        case NodeType::TableRow:
            return "TableRow";
        case NodeType::TableCell:
            return "TableCell";
        case NodeType::HtmlTag:
            return "HtmlTag";
        case NodeType::Paragraph:
            return "Paragraph";
        case NodeType::Comment:
            return "Comment";
        case NodeType::NoWiki:
            return "NoWiki";
        case NodeType::Redirect:
            return "Redirect";
        case NodeType::Category:
            return "Category";
        case NodeType::MagicWord:
            return "MagicWord";
        case NodeType::HorizontalRule:
            return "HorizontalRule";
        case NodeType::Document:
            return "Document";
    }
    return "Unknown";
}

// ============================================================================
// Base Node
// ============================================================================

std::string Node::to_plain_text() const {
    std::string result;
    for (const auto &child: children()) {
        result += child->to_plain_text();
    }
    return result;
}

// ============================================================================
// Leaf nodes
// ============================================================================

std::string CommentNode::to_wikitext() const {
    return "<!--" + content + "-->";
}

std::string NoWikiNode::to_wikitext() const {
    return "<nowiki>" + content + "</nowiki>";
}

std::string MagicWordNode::to_wikitext() const {
    return word;
}

// ============================================================================
// Container nodes
// ============================================================================

std::string FormattingNode::to_wikitext() const {
    std::string marker;
    switch (style) {
        case Style::Bold:
            marker = "'''";
            break;
        case Style::Italic:
            marker = "''";
            break;
        case Style::BoldItalic:
            marker = "'''''";
            break;
    }

    std::string result = marker;
    for (const auto &child: content) {
        result += child->to_wikitext();
    }
    result += marker;
    return result;
}

std::string LinkNode::full_target() const {
    if (anchor.empty()) {
        return target;
    }
    return target + "#" + anchor;
}

std::string LinkNode::to_wikitext() const {
    std::string result = "[[" + full_target();

    if (!display_content.empty()) {
        result += "|";
        for (const auto &child: display_content) {
            result += child->to_wikitext();
        }
    }

    result += "]]";
    return result;
}

std::string ExternalLinkNode::to_wikitext() const {
    if (!bracketed) {
        return url;
    }

    std::string result = "[" + url;

    if (!display_content.empty()) {
        result += " ";
        for (const auto &child: display_content) {
            result += child->to_wikitext();
        }
    }

    result += "]";
    return result;
}

std::string TemplateNode::to_wikitext() const {
    std::string result = "{{";
    if (is_parser_function) {
        result += "#";
    }
    result += name;

    for (const auto &param: parameters) {
        result += "|";
        if (param.name.has_value()) {
            result += param.name.value() + "=";
        }
        for (const auto &node: param.value) {
            result += node->to_wikitext();
        }
    }

    result += "}}";
    return result;
}

const TemplateParameter *TemplateNode::get_param(std::string_view name) const {
    for (const auto &param: parameters) {
        if (param.name.has_value() && param.name.value() == name) {
            return &param;
        }
    }
    return nullptr;
}

const TemplateParameter *TemplateNode::get_param(size_t position) const {
    size_t pos = 1;
    for (const auto &param: parameters) {
        if (!param.name.has_value()) {
            if (pos == position) {
                return &param;
            }
            ++pos;
        }
    }
    return nullptr;
}

std::string ParameterNode::to_wikitext() const {
    std::string result = "{{{" + name;

    if (!default_value.empty()) {
        result += "|";
        for (const auto &child: default_value) {
            result += child->to_wikitext();
        }
    }

    result += "}}}";
    return result;
}

std::string HeadingNode::to_wikitext() const {
    std::string marker(level, '=');
    std::string result = marker;

    for (const auto &child: content) {
        result += child->to_wikitext();
    }

    result += marker + "\n";
    return result;
}

std::string ListNode::to_wikitext() const {
    std::string result;
    for (const auto &item: items) {
        result += item->to_wikitext();
    }
    return result;
}

std::string ListItemNode::to_wikitext() const {
    std::string marker;
    if (is_definition_term) {
        marker = std::string(depth, ';');
    } else {
        // Determine marker based on parent list type - default to *
        marker = std::string(depth, '*');
    }

    std::string result = marker;
    for (const auto &child: content) {
        result += child->to_wikitext();
    }
    result += "\n";
    return result;
}

std::string TableCellNode::to_wikitext() const {
    std::string result = is_header ? "!" : "|";

    if (!attributes.empty()) {
        result += attributes + "|";
    }

    for (const auto &child: content) {
        result += child->to_wikitext();
    }

    return result;
}

std::string TableRowNode::to_wikitext() const {
    std::string result = "|-";
    if (!attributes.empty()) {
        result += " " + attributes;
    }
    result += "\n";

    for (const auto &cell: cells) {
        result += cell->to_wikitext() + "\n";
    }

    return result;
}

std::string TableNode::to_wikitext() const {
    std::string result = "{|";
    if (!attributes.empty()) {
        result += " " + attributes;
    }
    result += "\n";

    if (caption.has_value()) {
        result += "|+ " + caption.value() + "\n";
    }

    for (const auto &row: rows) {
        result += row->to_wikitext();
    }

    result += "|}\n";
    return result;
}

std::string HtmlTagNode::to_wikitext() const {
    std::string result = "<" + tag_name;

    for (const auto &[name, value]: attributes) {
        result += " " + name + "=\"" + value + "\"";
    }

    if (self_closing) {
        result += " />";
        return result;
    }

    result += ">";

    for (const auto &child: content) {
        result += child->to_wikitext();
    }

    result += "</" + tag_name + ">";
    return result;
}

std::optional<std::string_view> HtmlTagNode::get_attribute(std::string_view name) const {
    for (const auto &[attr_name, attr_value]: attributes) {
        if (attr_name == name) {
            return attr_value;
        }
    }
    return std::nullopt;
}

std::string ParagraphNode::to_wikitext() const {
    std::string result;
    for (const auto &child: content) {
        result += child->to_wikitext();
    }
    result += "\n\n";
    return result;
}

std::string RedirectNode::to_wikitext() const {
    return "#REDIRECT [[" + target + "]]\n";
}

std::string CategoryNode::to_wikitext() const {
    std::string result = "[[Category:" + category;
    if (!sort_key.empty()) {
        result += "|" + sort_key;
    }
    result += "]]";
    return result;
}

std::string DocumentNode::to_wikitext() const {
    std::string result;
    for (const auto &child: content) {
        result += child->to_wikitext();
    }
    return result;
}

// ============================================================================
// Factory functions
// ============================================================================

NodePtr make_text(std::string text) {
    return std::make_unique<TextNode>(std::move(text));
}

NodePtr make_text(std::string_view text) {
    return std::make_unique<TextNode>(std::string(text));
}

NodePtr make_formatting(FormattingNode::Style style, NodeList content) {
    auto node = std::make_unique<FormattingNode>();
    node->style = style;
    node->content = std::move(content);
    return node;
}

NodePtr make_link(std::string target) {
    auto node = std::make_unique<LinkNode>();
    node->target = std::move(target);
    return node;
}

NodePtr make_template(std::string name) {
    auto node = std::make_unique<TemplateNode>();
    node->name = std::move(name);
    return node;
}

NodePtr make_heading(int level, NodeList content) {
    auto node = std::make_unique<HeadingNode>();
    node->level = level;
    node->content = std::move(content);
    return node;
}

} // namespace wikilib::markup
