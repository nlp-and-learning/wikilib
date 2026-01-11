#include <gtest/gtest.h>
#include "wikilib/markup/ast.h"

using namespace wikilib::markup;

// ============================================================================
// Node type tests
// ============================================================================

TEST(AstTest, NodeTypeName) {
    EXPECT_EQ(node_type_name(NodeType::Text), "Text");
    EXPECT_EQ(node_type_name(NodeType::Link), "Link");
    EXPECT_EQ(node_type_name(NodeType::Template), "Template");
    EXPECT_EQ(node_type_name(NodeType::Heading), "Heading");
    EXPECT_EQ(node_type_name(NodeType::Document), "Document");
}

// ============================================================================
// TextNode tests
// ============================================================================

TEST(AstTest, TextNode_Creation) {
    TextNode node("Hello World");

    EXPECT_EQ(node.type, NodeType::Text);
    EXPECT_EQ(node.text, "Hello World");
    EXPECT_FALSE(node.text.empty());
}

TEST(AstTest, TextNode_ToPlainText) {
    TextNode node("Hello");
    EXPECT_EQ(node.to_plain_text(), "Hello");
}

TEST(AstTest, TextNode_Children) {
    TextNode node("test");
    EXPECT_TRUE(node.children().empty());
}

// ============================================================================
// FormattingNode tests
// ============================================================================

TEST(AstTest, FormattingNode_Bold) {
    FormattingNode node;
    node.style = FormattingNode::Style::Bold;

    EXPECT_EQ(node.type, NodeType::Formatting);
    EXPECT_EQ(node.style, FormattingNode::Style::Bold);
}

TEST(AstTest, FormattingNode_Italic) {
    FormattingNode node;
    node.style = FormattingNode::Style::Italic;

    EXPECT_EQ(node.style, FormattingNode::Style::Italic);
}

TEST(AstTest, FormattingNode_WithContent) {
    FormattingNode node;
    node.content.push_back(std::make_unique<TextNode>("bold text"));

    EXPECT_EQ(node.content.size(), 1u);
    EXPECT_EQ(node.content[0]->type, NodeType::Text);
}

// ============================================================================
// LinkNode tests
// ============================================================================

TEST(AstTest, LinkNode_Basic) {
    LinkNode node;
    node.target = "Page Title";

    EXPECT_EQ(node.type, NodeType::Link);
    EXPECT_EQ(node.target, "Page Title");
}

TEST(AstTest, LinkNode_FullTarget) {
    LinkNode node;
    node.target = "Main Page";
    EXPECT_EQ(node.full_target(), "Main Page");
}

TEST(AstTest, LinkNode_HasCustomDisplay) {
    LinkNode node;
    EXPECT_FALSE(node.has_custom_display());

    node.display_content.push_back(std::make_unique<TextNode>("Custom"));
    EXPECT_TRUE(node.has_custom_display());
}

// ============================================================================
// ExternalLinkNode tests
// ============================================================================

TEST(AstTest, ExternalLinkNode_Basic) {
    ExternalLinkNode node;
    node.url = "https://example.com";

    EXPECT_EQ(node.type, NodeType::ExternalLink);
    EXPECT_EQ(node.url, "https://example.com");
}

// ============================================================================
// TemplateNode tests
// ============================================================================

TEST(AstTest, TemplateNode_Basic) {
    TemplateNode node;
    node.name = "Infobox";

    EXPECT_EQ(node.type, NodeType::Template);
    EXPECT_EQ(node.name, "Infobox");
}

TEST(AstTest, TemplateNode_WithParameters) {
    TemplateNode node;

    TemplateParameter param1;
    param1.name = "param1";
    param1.value.push_back(std::make_unique<TextNode>("value1"));

    node.parameters.push_back(std::move(param1));

    EXPECT_EQ(node.parameters.size(), 1u);
    EXPECT_EQ(node.parameters[0].name, "param1");
}

// ============================================================================
// HeadingNode tests
// ============================================================================

TEST(AstTest, HeadingNode_Levels) {
    HeadingNode h1;
    h1.level = 1;
    EXPECT_EQ(h1.level, 1);

    HeadingNode h2;
    h2.level = 2;
    EXPECT_EQ(h2.level, 2);
}

TEST(AstTest, HeadingNode_Content) {
    HeadingNode node;
    node.level = 2;
    node.content.push_back(std::make_unique<TextNode>("Section Title"));

    EXPECT_EQ(node.content.size(), 1u);
}

// ============================================================================
// ListNode tests
// ============================================================================

TEST(AstTest, ListNode_Types) {
    ListNode bullet;
    bullet.list_type = ListNode::ListType::Bullet;
    EXPECT_EQ(bullet.list_type, ListNode::ListType::Bullet);

    ListNode numbered;
    numbered.list_type = ListNode::ListType::Numbered;
    EXPECT_EQ(numbered.list_type, ListNode::ListType::Numbered);
}

TEST(AstTest, ListNode_Items) {
    ListNode list;
    list.list_type = ListNode::ListType::Bullet;

    auto item1 = std::make_unique<ListItemNode>();
    auto item2 = std::make_unique<ListItemNode>();

    list.items.push_back(std::move(item1));
    list.items.push_back(std::move(item2));

    EXPECT_EQ(list.items.size(), 2u);
}

// ============================================================================
// ListItemNode tests
// ============================================================================

TEST(AstTest, ListItemNode_Depth) {
    ListItemNode item1;
    item1.depth = 1;
    EXPECT_EQ(item1.depth, 1);

    ListItemNode item2;
    item2.depth = 3;
    EXPECT_EQ(item2.depth, 3);
}

TEST(AstTest, ListItemNode_Content) {
    ListItemNode item;
    item.content.push_back(std::make_unique<TextNode>("Item text"));

    EXPECT_EQ(item.content.size(), 1u);
}

// ============================================================================
// TableNode tests
// ============================================================================

TEST(AstTest, TableNode_Basic) {
    TableNode table;

    EXPECT_EQ(table.type, NodeType::Table);
    EXPECT_FALSE(table.caption.has_value());
}

TEST(AstTest, TableNode_WithCaption) {
    TableNode table;
    table.caption = "Table Caption";

    ASSERT_TRUE(table.caption.has_value());
    EXPECT_EQ(*table.caption, "Table Caption");
}

TEST(AstTest, TableNode_Rows) {
    TableNode table;

    table.rows.push_back(std::make_unique<TableRowNode>());
    table.rows.push_back(std::make_unique<TableRowNode>());

    EXPECT_EQ(table.rows.size(), 2u);
}

// ============================================================================
// CommentNode tests
// ============================================================================

TEST(AstTest, CommentNode_Basic) {
    CommentNode node;
    node.content = "This is a comment";

    EXPECT_EQ(node.type, NodeType::Comment);
    EXPECT_EQ(node.content, "This is a comment");
}

// ============================================================================
// NoWikiNode tests
// ============================================================================

TEST(AstTest, NoWikiNode_Basic) {
    NoWikiNode node;
    node.content = "'''not bold'''";

    EXPECT_EQ(node.type, NodeType::NoWiki);
    EXPECT_EQ(node.content, "'''not bold'''");
}

// ============================================================================
// RedirectNode tests
// ============================================================================

TEST(AstTest, RedirectNode_Basic) {
    RedirectNode node;
    node.target = "Target Page";

    EXPECT_EQ(node.type, NodeType::Redirect);
    EXPECT_EQ(node.target, "Target Page");
}

// ============================================================================
// CategoryNode tests
// ============================================================================

TEST(AstTest, CategoryNode_Basic) {
    CategoryNode node;
    node.category = "Science";

    EXPECT_EQ(node.type, NodeType::Category);
    EXPECT_EQ(node.category, "Science");
    EXPECT_TRUE(node.sort_key.empty());
}

TEST(AstTest, CategoryNode_WithSortKey) {
    CategoryNode node;
    node.category = "People";
    node.sort_key = "Smith, John";

    EXPECT_EQ(node.category, "People");
    EXPECT_EQ(node.sort_key, "Smith, John");
}

// ============================================================================
// MagicWordNode tests
// ============================================================================

TEST(AstTest, MagicWordNode_Basic) {
    MagicWordNode node;
    node.word = "NOTOC";

    EXPECT_EQ(node.type, NodeType::MagicWord);
    EXPECT_EQ(node.word, "NOTOC");
}

// ============================================================================
// HtmlTagNode tests
// ============================================================================

TEST(AstTest, HtmlTagNode_Basic) {
    HtmlTagNode node;
    node.tag_name = "div";

    EXPECT_EQ(node.type, NodeType::HtmlTag);
    EXPECT_EQ(node.tag_name, "div");
    EXPECT_FALSE(node.self_closing);
}

TEST(AstTest, HtmlTagNode_SelfClosing) {
    HtmlTagNode node;
    node.self_closing = true;

    EXPECT_TRUE(node.self_closing);
}

TEST(AstTest, HtmlTagNode_Attributes) {
    HtmlTagNode node;
    node.attributes.push_back({"class", "container"});
    node.attributes.push_back({"id", "main"});

    EXPECT_EQ(node.attributes.size(), 2u);
    EXPECT_EQ(node.attributes[0].first, "class");
    EXPECT_EQ(node.attributes[0].second, "container");
}

// ============================================================================
// DocumentNode tests
// ============================================================================

TEST(AstTest, DocumentNode_Basic) {
    DocumentNode doc;

    EXPECT_EQ(doc.type, NodeType::Document);
    EXPECT_TRUE(doc.content.empty());
}

TEST(AstTest, DocumentNode_WithContent) {
    DocumentNode doc;

    doc.content.push_back(std::make_unique<TextNode>("Hello"));
    doc.content.push_back(std::make_unique<HeadingNode>());

    EXPECT_EQ(doc.content.size(), 2u);
    EXPECT_EQ(doc.content[0]->type, NodeType::Text);
    EXPECT_EQ(doc.content[1]->type, NodeType::Heading);
}

// ============================================================================
// Node hierarchy tests
// ============================================================================

TEST(AstTest, Node_Parent) {
    auto parent = std::make_unique<ParagraphNode>();
    auto child = std::make_unique<TextNode>("text");

    auto* child_ptr = child.get();
    parent->content.push_back(std::move(child));

    // Parent should be set
    child_ptr->parent = parent.get();
    EXPECT_EQ(child_ptr->parent, parent.get());
}

TEST(AstTest, Node_Children) {
    ParagraphNode para;

    EXPECT_TRUE(para.children().empty());

    para.content.push_back(std::make_unique<TextNode>("text"));

    EXPECT_FALSE(para.children().empty());
    EXPECT_EQ(para.children().size(), 1u);
}
