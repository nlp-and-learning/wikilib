#pragma once

/**
 * @file ast.hpp
 * @brief Abstract Syntax Tree nodes for parsed MediaWiki wikitext
 */

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "wikilib/core/types.h"

namespace wikilib::markup {

// Forward declarations
struct Node;
struct TextNode;
struct FormattingNode;
struct LinkNode;
struct ExternalLinkNode;
struct TemplateNode;
struct ParameterNode;
struct HeadingNode;
struct ListNode;
struct ListItemNode;
struct TableNode;
struct TableRowNode;
struct TableCellNode;
struct HtmlTagNode;
struct ParagraphNode;
struct CommentNode;
struct NoWikiNode;
struct RedirectNode;
struct CategoryNode;
struct MagicWordNode;
struct HorizontalRuleNode;

// ============================================================================
// Node pointer types
// ============================================================================

using NodePtr = std::unique_ptr<Node>;
using NodeList = std::vector<NodePtr>;

// ============================================================================
// Node types enum
// ============================================================================

enum class NodeType {
    Text,
    Formatting,
    Link,
    ExternalLink,
    Template,
    Parameter,
    Heading,
    List,
    ListItem,
    Table,
    TableRow,
    TableCell,
    HtmlTag,
    Paragraph,
    Comment,
    NoWiki,
    Redirect,
    Category,
    MagicWord,
    HorizontalRule,
    Document // Root node
};

/**
 * @brief Convert node type to string
 */
[[nodiscard]] std::string_view node_type_name(NodeType type) noexcept;

// ============================================================================
// Base node
// ============================================================================

/**
 * @brief Base class for all AST nodes
 */
struct Node {
    NodeType type;
    SourceRange location;
    Node *parent = nullptr;

    explicit Node(NodeType t) : type(t) {
    }

    virtual ~Node() = default;

    // Non-copyable, movable
    Node(const Node &) = delete;
    Node &operator=(const Node &) = delete;
    Node(Node &&) = default;
    Node &operator=(Node &&) = default;

    /**
     * @brief Get children of this node
     */
    [[nodiscard]] virtual NodeList &children() = 0;
    [[nodiscard]] virtual const NodeList &children() const = 0;

    /**
     * @brief Check if node has children
     */
    [[nodiscard]] bool has_children() const {
        return !children().empty();
    }

    /**
     * @brief Check node type
     */
    [[nodiscard]] bool is(NodeType t) const noexcept {
        return type == t;
    }

    /**
     * @brief Extract plain text from this node and children
     */
    [[nodiscard]] virtual std::string to_plain_text() const;

    /**
     * @brief Convert back to wikitext
     */
    [[nodiscard]] virtual std::string to_wikitext() const = 0;
};

// ============================================================================
// Leaf nodes (no children)
// ============================================================================

/**
 * @brief Plain text content
 */
struct TextNode : Node {
    std::string text;

    TextNode() : Node(NodeType::Text) {
    }

    explicit TextNode(std::string t) : Node(NodeType::Text), text(std::move(t)) {
    }

    [[nodiscard]] NodeList &children() override {
        return empty_children_;
    }

    [[nodiscard]] const NodeList &children() const override {
        return empty_children_;
    }

    [[nodiscard]] std::string to_plain_text() const override {
        return text;
    }

    [[nodiscard]] std::string to_wikitext() const override {
        return text;
    }
private:
    static inline NodeList empty_children_{};
};

/**
 * @brief HTML comment
 */
struct CommentNode : Node {
    std::string content;

    CommentNode() : Node(NodeType::Comment) {
    }

    [[nodiscard]] NodeList &children() override {
        return empty_children_;
    }

    [[nodiscard]] const NodeList &children() const override {
        return empty_children_;
    }

    [[nodiscard]] std::string to_plain_text() const override {
        return "";
    }

    [[nodiscard]] std::string to_wikitext() const override;
private:
    static inline NodeList empty_children_{};
};

/**
 * @brief Nowiki content (preserved as-is)
 */
struct NoWikiNode : Node {
    std::string content;

    NoWikiNode() : Node(NodeType::NoWiki) {
    }

    [[nodiscard]] NodeList &children() override {
        return empty_children_;
    }

    [[nodiscard]] const NodeList &children() const override {
        return empty_children_;
    }

    [[nodiscard]] std::string to_plain_text() const override {
        return content;
    }

    [[nodiscard]] std::string to_wikitext() const override;
private:
    static inline NodeList empty_children_{};
};

/**
 * @brief Magic word (__TOC__, etc.)
 */
struct MagicWordNode : Node {
    std::string word;

    MagicWordNode() : Node(NodeType::MagicWord) {
    }

    [[nodiscard]] NodeList &children() override {
        return empty_children_;
    }

    [[nodiscard]] const NodeList &children() const override {
        return empty_children_;
    }

    [[nodiscard]] std::string to_plain_text() const override {
        return "";
    }

    [[nodiscard]] std::string to_wikitext() const override;
private:
    static inline NodeList empty_children_{};
};

/**
 * @brief Horizontal rule (----)
 */
struct HorizontalRuleNode : Node {
    HorizontalRuleNode() : Node(NodeType::HorizontalRule) {
    }

    [[nodiscard]] NodeList &children() override {
        return empty_children_;
    }

    [[nodiscard]] const NodeList &children() const override {
        return empty_children_;
    }

    [[nodiscard]] std::string to_plain_text() const override {
        return "\n";
    }

    [[nodiscard]] std::string to_wikitext() const override {
        return "----\n";
    }
private:
    static inline NodeList empty_children_{};
};

// ============================================================================
// Container nodes
// ============================================================================

/**
 * @brief Text formatting (bold, italic)
 */
struct FormattingNode : Node {
    enum class Style { Bold, Italic, BoldItalic };
    Style style = Style::Bold;
    NodeList content;

    FormattingNode() : Node(NodeType::Formatting) {
    }

    [[nodiscard]] NodeList &children() override {
        return content;
    }

    [[nodiscard]] const NodeList &children() const override {
        return content;
    }

    [[nodiscard]] std::string to_wikitext() const override;
};

/**
 * @brief Internal wiki link [[target|display]]
 */
struct LinkNode : Node {
    std::string target;
    std::string anchor; // Section link (#anchor)
    NodeList display_content; // If empty, display target

    LinkNode() : Node(NodeType::Link) {
    }

    [[nodiscard]] NodeList &children() override {
        return display_content;
    }

    [[nodiscard]] const NodeList &children() const override {
        return display_content;
    }

    [[nodiscard]] std::string to_wikitext() const override;

    [[nodiscard]] std::string full_target() const;

    [[nodiscard]] bool has_custom_display() const {
        return !display_content.empty();
    }
};

/**
 * @brief External link [url text]
 */
struct ExternalLinkNode : Node {
    std::string url;
    NodeList display_content;
    bool bracketed = true; // false for bare URLs

    ExternalLinkNode() : Node(NodeType::ExternalLink) {
    }

    [[nodiscard]] NodeList &children() override {
        return display_content;
    }

    [[nodiscard]] const NodeList &children() const override {
        return display_content;
    }

    [[nodiscard]] std::string to_wikitext() const override;
};

/**
 * @brief Template parameter
 */
struct TemplateParameter {
    std::optional<std::string> name; // Empty for positional parameters
    NodeList value;

    [[nodiscard]] bool is_positional() const {
        return !name.has_value();
    }
};

/**
 * @brief Template invocation {{name|params}}
 */
struct TemplateNode : Node {
    std::string name;
    std::vector<TemplateParameter> parameters;
    bool is_parser_function = false; // {{#if:...}}

    TemplateNode() : Node(NodeType::Template) {
    }

    [[nodiscard]] NodeList &children() override {
        return all_children_;
    }

    [[nodiscard]] const NodeList &children() const override {
        return all_children_;
    }

    [[nodiscard]] std::string to_wikitext() const override;

    /**
     * @brief Get parameter by name
     */
    [[nodiscard]] const TemplateParameter *get_param(std::string_view name) const;

    /**
     * @brief Get parameter by position (1-indexed)
     */
    [[nodiscard]] const TemplateParameter *get_param(size_t position) const;
private:
    NodeList all_children_; // Flattened list for tree traversal
};

/**
 * @brief Template parameter reference {{{name|default}}}
 */
struct ParameterNode : Node {
    std::string name;
    NodeList default_value;

    ParameterNode() : Node(NodeType::Parameter) {
    }

    [[nodiscard]] NodeList &children() override {
        return default_value;
    }

    [[nodiscard]] const NodeList &children() const override {
        return default_value;
    }

    [[nodiscard]] std::string to_wikitext() const override;
};

/**
 * @brief Section heading
 */
struct HeadingNode : Node {
    int level = 2; // 1-6
    NodeList content;

    HeadingNode() : Node(NodeType::Heading) {
    }

    [[nodiscard]] NodeList &children() override {
        return content;
    }

    [[nodiscard]] const NodeList &children() const override {
        return content;
    }

    [[nodiscard]] std::string to_wikitext() const override;
};

/**
 * @brief List (bulleted, numbered, definition)
 */
struct ListNode : Node {
    enum class ListType { Bullet, Numbered, Definition };
    ListType list_type = ListType::Bullet;
    NodeList items;

    ListNode() : Node(NodeType::List) {
    }

    [[nodiscard]] NodeList &children() override {
        return items;
    }

    [[nodiscard]] const NodeList &children() const override {
        return items;
    }

    [[nodiscard]] std::string to_wikitext() const override;
};

/**
 * @brief Single list item
 */
struct ListItemNode : Node {
    int depth = 1;
    bool is_definition_term = false; // ; vs :
    NodeList content;

    ListItemNode() : Node(NodeType::ListItem) {
    }

    [[nodiscard]] NodeList &children() override {
        return content;
    }

    [[nodiscard]] const NodeList &children() const override {
        return content;
    }

    [[nodiscard]] std::string to_wikitext() const override;
};

/**
 * @brief Table cell
 */
struct TableCellNode : Node {
    bool is_header = false; // th vs td
    std::string attributes; // Cell attributes
    NodeList content;

    TableCellNode() : Node(NodeType::TableCell) {
    }

    [[nodiscard]] NodeList &children() override {
        return content;
    }

    [[nodiscard]] const NodeList &children() const override {
        return content;
    }

    [[nodiscard]] std::string to_wikitext() const override;
};

/**
 * @brief Table row
 */
struct TableRowNode : Node {
    std::string attributes;
    NodeList cells;

    TableRowNode() : Node(NodeType::TableRow) {
    }

    [[nodiscard]] NodeList &children() override {
        return cells;
    }

    [[nodiscard]] const NodeList &children() const override {
        return cells;
    }

    [[nodiscard]] std::string to_wikitext() const override;
};

/**
 * @brief Table
 */
struct TableNode : Node {
    std::string attributes;
    std::optional<std::string> caption;
    NodeList rows;

    TableNode() : Node(NodeType::Table) {
    }

    [[nodiscard]] NodeList &children() override {
        return rows;
    }

    [[nodiscard]] const NodeList &children() const override {
        return rows;
    }

    [[nodiscard]] std::string to_wikitext() const override;
};

/**
 * @brief HTML tag
 */
struct HtmlTagNode : Node {
    std::string tag_name;
    std::vector<std::pair<std::string, std::string>> attributes;
    NodeList content;
    bool self_closing = false;

    HtmlTagNode() : Node(NodeType::HtmlTag) {
    }

    [[nodiscard]] NodeList &children() override {
        return content;
    }

    [[nodiscard]] const NodeList &children() const override {
        return content;
    }

    [[nodiscard]] std::string to_wikitext() const override;

    [[nodiscard]] std::optional<std::string_view> get_attribute(std::string_view name) const;
};

/**
 * @brief Paragraph (implicit block element)
 */
struct ParagraphNode : Node {
    NodeList content;

    ParagraphNode() : Node(NodeType::Paragraph) {
    }

    [[nodiscard]] NodeList &children() override {
        return content;
    }

    [[nodiscard]] const NodeList &children() const override {
        return content;
    }

    [[nodiscard]] std::string to_wikitext() const override;
};

/**
 * @brief Redirect
 */
struct RedirectNode : Node {
    std::string target;

    RedirectNode() : Node(NodeType::Redirect) {
    }

    [[nodiscard]] NodeList &children() override {
        return empty_children_;
    }

    [[nodiscard]] const NodeList &children() const override {
        return empty_children_;
    }

    [[nodiscard]] std::string to_plain_text() const override {
        return "";
    }

    [[nodiscard]] std::string to_wikitext() const override;
private:
    static inline NodeList empty_children_{};
};

/**
 * @brief Category link [[Category:Name]]
 */
struct CategoryNode : Node {
    std::string category;
    std::string sort_key;

    CategoryNode() : Node(NodeType::Category) {
    }

    [[nodiscard]] NodeList &children() override {
        return empty_children_;
    }

    [[nodiscard]] const NodeList &children() const override {
        return empty_children_;
    }

    [[nodiscard]] std::string to_plain_text() const override {
        return "";
    }

    [[nodiscard]] std::string to_wikitext() const override;
private:
    static inline NodeList empty_children_{};
};

// ============================================================================
// Document root
// ============================================================================

/**
 * @brief Root document node
 */
struct DocumentNode : Node {
    NodeList content;
    std::vector<CategoryNode *> categories; // Non-owning pointers
    RedirectNode *redirect = nullptr; // Non-owning pointer

    DocumentNode() : Node(NodeType::Document) {
    }

    [[nodiscard]] NodeList &children() override {
        return content;
    }

    [[nodiscard]] const NodeList &children() const override {
        return content;
    }

    [[nodiscard]] std::string to_wikitext() const override;

    [[nodiscard]] bool is_redirect() const {
        return redirect != nullptr;
    }
};

// ============================================================================
// Factory functions
// ============================================================================

[[nodiscard]] NodePtr make_text(std::string text);
[[nodiscard]] NodePtr make_text(std::string_view text);
[[nodiscard]] NodePtr make_formatting(FormattingNode::Style style, NodeList content);
[[nodiscard]] NodePtr make_link(std::string target);
[[nodiscard]] NodePtr make_template(std::string name);
[[nodiscard]] NodePtr make_heading(int level, NodeList content);

} // namespace wikilib::markup
