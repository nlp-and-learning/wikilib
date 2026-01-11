#pragma once

/**
 * @file wikitext_visitor.hpp
 * @brief Visitor pattern for traversing wikitext AST
 */

#include <functional>
#include "wikilib/markup/ast.h"

namespace wikilib::markup {

// ============================================================================
// Visitor interface
// ============================================================================

/**
 * @brief Abstract visitor for AST nodes
 *
 * Override methods for node types you care about.
 * Default implementations visit children recursively.
 */
class WikitextVisitor {
public:
    virtual ~WikitextVisitor() = default;

    /**
     * @brief Visit any node (dispatches to specific method)
     */
    virtual void visit(Node &node);

    /**
     * @brief Called before visiting node's children
     * @return false to skip children
     */
    virtual bool enter(Node &node) {
        (void) node;
        return true;
    }

    /**
     * @brief Called after visiting node's children
     */
    virtual void leave(Node &node) {
        (void) node;
    }

    // Specific node type visitors
    virtual void visit(TextNode &node);
    virtual void visit(FormattingNode &node);
    virtual void visit(LinkNode &node);
    virtual void visit(ExternalLinkNode &node);
    virtual void visit(TemplateNode &node);
    virtual void visit(ParameterNode &node);
    virtual void visit(HeadingNode &node);
    virtual void visit(ListNode &node);
    virtual void visit(ListItemNode &node);
    virtual void visit(TableNode &node);
    virtual void visit(TableRowNode &node);
    virtual void visit(TableCellNode &node);
    virtual void visit(HtmlTagNode &node);
    virtual void visit(ParagraphNode &node);
    virtual void visit(CommentNode &node);
    virtual void visit(NoWikiNode &node);
    virtual void visit(RedirectNode &node);
    virtual void visit(CategoryNode &node);
    virtual void visit(MagicWordNode &node);
    virtual void visit(HorizontalRuleNode &node);
    virtual void visit(DocumentNode &node);
protected:
    /**
     * @brief Visit all children of a node
     */
    void visit_children(Node &node);
};

// ============================================================================
// Const visitor
// ============================================================================

/**
 * @brief Const visitor for read-only traversal
 */
class ConstWikitextVisitor {
public:
    virtual ~ConstWikitextVisitor() = default;

    virtual void visit(const Node &node);

    virtual bool enter(const Node &node) {
        (void) node;
        return true;
    }

    virtual void leave(const Node &node) {
        (void) node;
    }

    virtual void visit(const TextNode &node);
    virtual void visit(const FormattingNode &node);
    virtual void visit(const LinkNode &node);
    virtual void visit(const ExternalLinkNode &node);
    virtual void visit(const TemplateNode &node);
    virtual void visit(const ParameterNode &node);
    virtual void visit(const HeadingNode &node);
    virtual void visit(const ListNode &node);
    virtual void visit(const ListItemNode &node);
    virtual void visit(const TableNode &node);
    virtual void visit(const TableRowNode &node);
    virtual void visit(const TableCellNode &node);
    virtual void visit(const HtmlTagNode &node);
    virtual void visit(const ParagraphNode &node);
    virtual void visit(const CommentNode &node);
    virtual void visit(const NoWikiNode &node);
    virtual void visit(const RedirectNode &node);
    virtual void visit(const CategoryNode &node);
    virtual void visit(const MagicWordNode &node);
    virtual void visit(const HorizontalRuleNode &node);
    virtual void visit(const DocumentNode &node);
protected:
    void visit_children(const Node &node);
};

// ============================================================================
// Functional visitors
// ============================================================================

/**
 * @brief Callback type for node visiting
 */
using NodeCallback = std::function<bool(Node &)>;
using ConstNodeCallback = std::function<bool(const Node &)>;

/**
 * @brief Visit all nodes with callback
 * @param root Root node to start from
 * @param callback Called for each node; return false to stop
 * @return true if completed, false if stopped early
 */
bool walk(Node &root, NodeCallback callback);
bool walk(const Node &root, ConstNodeCallback callback);

/**
 * @brief Visit nodes of specific type
 */
template<typename NodeType>
void walk_type(Node &root, std::function<void(NodeType &)> callback) {
    walk(root, [&callback](Node &node) {
        if (auto *typed = dynamic_cast<NodeType *>(&node)) {
            callback(*typed);
        }
        return true;
    });
}

template<typename NodeType>
void walk_type(const Node &root, std::function<void(const NodeType &)> callback) {
    walk(root, [&callback](const Node &node) {
        if (const auto *typed = dynamic_cast<const NodeType *>(&node)) {
            callback(*typed);
        }
        return true;
    });
}

// ============================================================================
// Common visitors
// ============================================================================

/**
 * @brief Extract all links from document
 */
class LinkExtractor : public ConstWikitextVisitor {
public:
    using ConstWikitextVisitor::visit;

    struct ExtractedLink {
        std::string target;
        std::string display;
        bool is_external = false;
        SourceRange location;
    };

    void visit(const LinkNode &node) override;
    void visit(const ExternalLinkNode &node) override;

    [[nodiscard]] const std::vector<ExtractedLink> &links() const {
        return links_;
    }
private:
    std::vector<ExtractedLink> links_;
};

/**
 * @brief Extract all templates from document
 */
class TemplateExtractor : public ConstWikitextVisitor {
public:
    using ConstWikitextVisitor::visit;

    struct ExtractedTemplate {
        std::string name;
        std::vector<std::pair<std::string, std::string>> parameters;
        SourceRange location;
    };

    void visit(const TemplateNode &node) override;

    [[nodiscard]] const std::vector<ExtractedTemplate> &templates() const {
        return templates_;
    }
private:
    std::vector<ExtractedTemplate> templates_;
};

/**
 * @brief Extract all categories from document
 */
class CategoryExtractor : public ConstWikitextVisitor {
public:
    using ConstWikitextVisitor::visit;

    struct ExtractedCategory {
        std::string name;
        std::string sort_key;
    };

    void visit(const CategoryNode &node) override;

    [[nodiscard]] const std::vector<ExtractedCategory> &categories() const {
        return categories_;
    }
private:
    std::vector<ExtractedCategory> categories_;
};

/**
 * @brief Extract section structure
 */
class SectionExtractor : public ConstWikitextVisitor {
public:
    using ConstWikitextVisitor::visit;

    struct Section {
        int level;
        std::string title;
        SourceRange location;
        std::vector<Section> subsections;
    };

    void visit(const HeadingNode &node) override;

    [[nodiscard]] const std::vector<Section> &sections() const {
        return root_sections_;
    }
private:
    std::vector<Section> root_sections_;
    std::vector<Section *> section_stack_;
};

/**
 * @brief Extract plain text content
 */
class PlainTextExtractor : public ConstWikitextVisitor {
public:
    using ConstWikitextVisitor::visit;

    void visit(const TextNode &node) override;
    void visit(const NoWikiNode &node) override;
    bool enter(const Node &node) override;
    void leave(const Node &node) override;

    [[nodiscard]] std::string text() const {
        return text_;
    }
private:
    std::string text_;
    bool add_space_ = false;
};

// ============================================================================
// Transforming visitors
// ============================================================================

/**
 * @brief Remove templates from AST
 */
class TemplateRemover : public WikitextVisitor {
public:
    using WikitextVisitor::visit;

    bool enter(Node &node) override;
};

/**
 * @brief Strip all formatting, keeping only text
 */
class FormattingStripper : public WikitextVisitor {
public:
    using WikitextVisitor::visit;

    void visit(FormattingNode &node) override;
};

} // namespace wikilib::markup
