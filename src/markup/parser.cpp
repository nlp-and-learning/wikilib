/**
 * @file parser.cpp
 * @brief Implementation of MediaWiki wikitext parser
 */

#include "wikilib/markup/parser.h"
#include <algorithm>
#include <sstream>
#include "wikilib/core/types.h"
#include "wikilib/markup/ast.h"
#include "wikilib/markup/tokenizer.h"

namespace wikilib::markup {

// ============================================================================
// Parser implementation
// ============================================================================

Parser::Parser(ParserConfig config) : config_(std::move(config)) {
}

ParseResult Parser::parse(std::string_view input) {
    return parse(input, PageInfo{});
}

ParseResult Parser::parse(std::string_view input, const PageInfo &page) {
    tokenizer_ = std::make_unique<Tokenizer>(input, config_.tokenizer);
    errors_.clear();
    depth_ = 0;

    auto doc = std::make_unique<DocumentNode>();

    // Parse content
    doc->content = parse_content();

    // Collect categories and check for redirect
    for (const auto &node: doc->content) {
        if (node->type == NodeType::Category) {
            doc->categories.push_back(static_cast<CategoryNode *>(node.get()));
        } else if (node->type == NodeType::Redirect) {
            doc->redirect = static_cast<RedirectNode *>(node.get());
        }
    }

    // Add tokenizer errors
    for (const auto &err: tokenizer_->errors()) {
        errors_.push_back(err);
    }

    ParseResult result;
    result.document = std::move(doc);
    result.errors = std::move(errors_);

    return result;
}

NodeList Parser::parse_inline(std::string_view input) {
    tokenizer_ = std::make_unique<Tokenizer>(input, config_.tokenizer);
    errors_.clear();
    depth_ = 0;

    return parse_inline_content();
}

std::vector<TemplateParameter> Parser::parse_template_params(std::string_view input) {
    tokenizer_ = std::make_unique<Tokenizer>(input, config_.tokenizer);
    errors_.clear();
    depth_ = 0;

    std::vector<TemplateParameter> params;

    while (!at_end()) {
        TemplateParameter param;

        // Check for named parameter (name=value)
        NodeList name_or_value;
        bool found_equals = false;

        while (!at_end() && !check(TokenType::Pipe)) {
            if (check(TokenType::Equals) && !found_equals) {
                found_equals = true;
                // Everything before = is the name
                std::string name;
                for (const auto &node: name_or_value) {
                    name += node->to_plain_text();
                }
                // Trim whitespace
                size_t start = name.find_first_not_of(" \t");
                size_t end = name.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos) {
                    param.name = name.substr(start, end - start + 1);
                } else {
                    param.name = name;
                }
                name_or_value.clear();
                advance(); // skip =
            } else {
                auto node = parse_node();
                if (node) {
                    name_or_value.push_back(std::move(node));
                } else {
                    advance();
                }
            }
        }

        if (found_equals) {
            param.value = std::move(name_or_value);
        } else {
            param.value = std::move(name_or_value);
        }

        params.push_back(std::move(param));

        if (check(TokenType::Pipe)) {
            advance(); // skip |
        }
    }

    return params;
}

// ============================================================================
// Content parsing
// ============================================================================

NodeList Parser::parse_content() {
    NodeList nodes;

    while (!at_end()) {
        auto node = parse_node();
        if (node) {
            nodes.push_back(std::move(node));
        }
    }

    return nodes;
}

NodeList Parser::parse_block_content() {
    NodeList nodes;

    while (!at_end()) {
        // Stop at block-ending tokens
        if (check(TokenType::TableEnd) || check(TokenType::TableRowStart)) {
            break;
        }

        auto node = parse_node();
        if (node) {
            nodes.push_back(std::move(node));
        }
    }

    return nodes;
}

NodeList Parser::parse_inline_content() {
    NodeList nodes;

    while (!at_end()) {
        // Stop at block-level tokens
        if (check(TokenType::Newline) || check(TokenType::Heading) || check(TokenType::TableStart) ||
            check(TokenType::TableEnd) || check(TokenType::BulletList) || check(TokenType::NumberedList)) {
            break;
        }

        auto node = parse_node();
        if (node) {
            nodes.push_back(std::move(node));
        } else {
            break;
        }
    }

    return nodes;
}

// ============================================================================
// Node parsing
// ============================================================================

NodePtr Parser::parse_node() {
    if (at_end()) {
        return nullptr;
    }

    DepthGuard guard(*this);
    if (guard.exceeded()) {
        add_error("Maximum nesting depth exceeded", ErrorSeverity::Error);
        return nullptr;
    }

    const Token &tok = current();

    switch (tok.type) {
        case TokenType::Text:
        case TokenType::Whitespace: {
            auto node = std::make_unique<TextNode>(std::string(tok.text));
            node->location = tok.location;
            advance();
            return node;
        }

        case TokenType::Newline:
            advance();
            return std::make_unique<TextNode>("\n");

        case TokenType::Bold:
        case TokenType::Italic:
        case TokenType::BoldItalic:
            return parse_formatting();

        case TokenType::LinkOpen:
        case TokenType::Category:
            return parse_link();

        case TokenType::ExternalLinkOpen:
            return parse_external_link();

        case TokenType::TemplateOpen:
        case TokenType::ParserFunction:
            return parse_template();

        case TokenType::ParameterOpen:
            return parse_parameter();

        case TokenType::Heading:
            return parse_heading();

        case TokenType::BulletList:
        case TokenType::NumberedList:
        case TokenType::DefinitionTerm:
        case TokenType::DefinitionDesc:
            if (config_.parse_lists) {
                return parse_list();
            }
            advance();
            return std::make_unique<TextNode>(std::string(tok.text));

        case TokenType::TableStart:
            if (config_.parse_tables) {
                return parse_table();
            }
            advance();
            return std::make_unique<TextNode>(std::string(tok.text));

        case TokenType::HtmlTagOpen:
            return parse_html_tag();

        case TokenType::HtmlComment: {
            auto node = std::make_unique<CommentNode>();
            // Extract content between <!-- and -->
            std::string_view content = tok.text;
            if (content.size() > 7) { // "<!--" + "-->"
                content = content.substr(4, content.size() - 7);
            }
            node->content = std::string(content);
            node->location = tok.location;
            advance();
            return node;
        }

        case TokenType::NoWiki: {
            auto node = std::make_unique<NoWikiNode>();
            std::string_view content = tok.text;
            // Extract content between <nowiki> and </nowiki>
            if (content.starts_with("<nowiki>") && content.ends_with("</nowiki>")) {
                content = content.substr(8, content.size() - 17);
            }
            node->content = std::string(content);
            node->location = tok.location;
            advance();
            return node;
        }

        case TokenType::HorizontalRule: {
            auto node = std::make_unique<HorizontalRuleNode>();
            node->location = tok.location;
            advance();
            return node;
        }

        case TokenType::MagicWord: {
            auto node = std::make_unique<MagicWordNode>();
            node->word = std::string(tok.text);
            node->location = tok.location;
            advance();
            return node;
        }

        case TokenType::Redirect: {
            auto node = std::make_unique<RedirectNode>();
            node->location = tok.location;
            advance();

            // Skip whitespace
            while (check(TokenType::Whitespace)) {
                advance();
            }

            // Expect [[ link
            if (check(TokenType::LinkOpen)) {
                advance();
                // Read target until ]] or |
                std::string target;
                while (!at_end() && !check(TokenType::LinkClose) && !check(TokenType::LinkSeparator)) {
                    target += current().text;
                    advance();
                }
                node->target = target;

                if (check(TokenType::LinkClose)) {
                    advance();
                }
            }

            return node;
        }

        case TokenType::Pipe:
        case TokenType::Equals:
            // These are usually consumed by higher-level constructs
            // If we see them here, treat as text
            {
                auto node = std::make_unique<TextNode>(std::string(tok.text));
                node->location = tok.location;
                advance();
                return node;
            }

        case TokenType::LinkClose:
        case TokenType::TemplateClose:
        case TokenType::ParameterClose:
        case TokenType::ExternalLinkClose:
        case TokenType::HtmlTagClose:
            // End markers - return nullptr to signal parent to stop
            return nullptr;

        case TokenType::TableEnd:
        case TokenType::TableRowStart:
            // Table markers - end marker when parsing tables, otherwise treat as text
            if (config_.parse_tables) {
                return nullptr;
            }
            advance();
            return std::make_unique<TextNode>(std::string(tok.text));

        default:
            // Unknown token - treat as text
            {
                auto node = std::make_unique<TextNode>(std::string(tok.text));
                node->location = tok.location;
                advance();
                return node;
            }
    }
}

NodePtr Parser::parse_paragraph() {
    auto para = std::make_unique<ParagraphNode>();
    SourcePosition start = current().location.begin;

    para->content = parse_inline_content();

    if (!para->content.empty()) {
        para->location = {start, para->content.back()->location.end};
    }

    return para;
}

NodePtr Parser::parse_heading() {
    auto heading = std::make_unique<HeadingNode>();
    const Token &tok = current();
    heading->level = tok.level;
    heading->location.begin = tok.location.begin;

    advance(); // skip opening ==

    // Parse content until matching closing ==
    while (!at_end() && !check(TokenType::Heading) && !check(TokenType::Newline)) {
        auto node = parse_node();
        if (node) {
            heading->content.push_back(std::move(node));
        } else {
            break;
        }
    }

    // Skip closing == if present
    if (check(TokenType::Heading)) {
        heading->location.end = current().location.end;
        advance();
    }

    // Skip newline after heading
    if (check(TokenType::Newline)) {
        advance();
    }

    return heading;
}

NodePtr Parser::parse_list() {
    auto list = std::make_unique<ListNode>();
    const Token &first = current();

    // Determine list type from first marker
    switch (first.type) {
        case TokenType::BulletList:
            list->list_type = ListNode::ListType::Bullet;
            break;
        case TokenType::NumberedList:
            list->list_type = ListNode::ListType::Numbered;
            break;
        case TokenType::DefinitionTerm:
        case TokenType::DefinitionDesc:
            list->list_type = ListNode::ListType::Definition;
            break;
        default:
            break;
    }

    list->location.begin = first.location.begin;

    // Parse list items
    while (!at_end() && (check(TokenType::BulletList) || check(TokenType::NumberedList) ||
                         check(TokenType::DefinitionTerm) || check(TokenType::DefinitionDesc))) {
        auto item = parse_list_item();
        if (item) {
            list->items.push_back(std::move(item));
        }

        // Skip newline between items
        if (check(TokenType::Newline)) {
            advance();
        } else {
            break;
        }
    }

    if (!list->items.empty()) {
        list->location.end = list->items.back()->location.end;
    }

    return list;
}

NodePtr Parser::parse_list_item() {
    auto item = std::make_unique<ListItemNode>();
    const Token &tok = current();

    item->depth = tok.level;
    item->is_definition_term = (tok.type == TokenType::DefinitionTerm);
    item->location.begin = tok.location.begin;

    advance(); // skip list marker

    // Parse item content until newline
    while (!at_end() && !check(TokenType::Newline)) {
        auto node = parse_node();
        if (node) {
            item->content.push_back(std::move(node));
        } else {
            break;
        }
    }

    if (!item->content.empty()) {
        item->location.end = item->content.back()->location.end;
    }

    return item;
}

NodePtr Parser::parse_table() {
    auto table = std::make_unique<TableNode>();
    table->location.begin = current().location.begin;

    advance(); // skip {|

    // Parse table attributes (rest of first line)
    std::string attrs;
    while (!at_end() && !check(TokenType::Newline)) {
        attrs += current().text;
        advance();
    }
    table->attributes = attrs;

    if (check(TokenType::Newline)) {
        advance();
    }

    // Parse table content
    while (!at_end() && !check(TokenType::TableEnd)) {
        if (check(TokenType::TableCaption)) {
            advance(); // skip |+
            std::string caption;
            while (!at_end() && !check(TokenType::Newline)) {
                caption += current().text;
                advance();
            }
            table->caption = caption;
            if (check(TokenType::Newline)) {
                advance();
            }
        } else if (check(TokenType::TableRowStart)) {
            auto row = parse_table_row();
            if (row) {
                table->rows.push_back(std::move(row));
            }
        } else if (check(TokenType::TableHeaderCell) || check(TokenType::TableDataCell)) {
            // Implicit first row
            auto row = std::make_unique<TableRowNode>();
            row->location.begin = current().location.begin;

            while (!at_end() && !check(TokenType::TableEnd) && !check(TokenType::TableRowStart)) {
                auto cell = parse_table_cell();
                if (cell) {
                    row->cells.push_back(std::move(cell));
                }
                if (check(TokenType::Newline)) {
                    advance();
                }
            }

            if (!row->cells.empty()) {
                row->location.end = row->cells.back()->location.end;
                table->rows.push_back(std::move(row));
            }
        } else {
            advance(); // skip unknown tokens in table
        }
    }

    if (check(TokenType::TableEnd)) {
        table->location.end = current().location.end;
        advance();
    }

    return table;
}

NodePtr Parser::parse_table_row() {
    auto row = std::make_unique<TableRowNode>();
    row->location.begin = current().location.begin;

    advance(); // skip |-

    // Parse row attributes
    std::string attrs;
    while (!at_end() && !check(TokenType::Newline)) {
        attrs += current().text;
        advance();
    }
    row->attributes = attrs;

    if (check(TokenType::Newline)) {
        advance();
    }

    // Parse cells until next row or table end
    while (!at_end() && !check(TokenType::TableEnd) && !check(TokenType::TableRowStart)) {
        if (check(TokenType::TableHeaderCell) || check(TokenType::TableDataCell)) {
            auto cell = parse_table_cell();
            if (cell) {
                row->cells.push_back(std::move(cell));
            }
        } else if (check(TokenType::Newline)) {
            advance();
        } else {
            break;
        }
    }

    if (!row->cells.empty()) {
        row->location.end = row->cells.back()->location.end;
    }

    return row;
}

NodePtr Parser::parse_table_cell() {
    auto cell = std::make_unique<TableCellNode>();
    cell->location.begin = current().location.begin;
    cell->is_header = check(TokenType::TableHeaderCell);

    advance(); // skip | or !

    // Check for cell attributes (text | more text)
    // First, check if there's a | before newline
    NodeList content;
    bool found_separator = false;

    while (!at_end() && !check(TokenType::Newline) && !check(TokenType::TableHeaderCell) &&
           !check(TokenType::TableDataCell) && !check(TokenType::TableRowStart) && !check(TokenType::TableEnd)) {
        if (check(TokenType::Pipe) && !found_separator) {
            found_separator = true;
            // Everything before was attributes
            std::string attrs;
            for (const auto &node: content) {
                attrs += node->to_plain_text();
            }
            cell->attributes = attrs;
            content.clear();
            advance();
        } else {
            auto node = parse_node();
            if (node) {
                content.push_back(std::move(node));
            } else {
                break;
            }
        }
    }

    cell->content = std::move(content);

    if (!cell->content.empty()) {
        cell->location.end = cell->content.back()->location.end;
    }

    return cell;
}

NodePtr Parser::parse_template() {
    auto tmpl = std::make_unique<TemplateNode>();
    tmpl->location.begin = current().location.begin;
    tmpl->is_parser_function = check(TokenType::ParserFunction);

    advance(); // skip {{ or {{#

    // Skip leading whitespace
    while (check(TokenType::Whitespace) || check(TokenType::Newline)) {
        advance();
    }

    // Parse template name until | or }}
    std::string name;
    while (!at_end() && !check(TokenType::Pipe) && !check(TokenType::TemplateClose)) {
        name += current().text;
        advance();
    }

    // Trim whitespace from name
    size_t start = name.find_first_not_of(" \t\n");
    size_t end = name.find_last_not_of(" \t\n");
    if (start != std::string::npos && end != std::string::npos) {
        tmpl->name = name.substr(start, end - start + 1);
    } else {
        tmpl->name = name;
    }

    // Parse parameters
    if (check(TokenType::Pipe)) {
        advance(); // skip first |

        while (!at_end() && !check(TokenType::TemplateClose)) {
            TemplateParameter param;
            NodeList value_nodes;
            std::string potential_name;
            bool found_equals = false;

            while (!at_end() && !check(TokenType::Pipe) && !check(TokenType::TemplateClose)) {
                if (check(TokenType::Equals) && !found_equals) {
                    found_equals = true;
                    // Everything before is the name
                    for (const auto &node: value_nodes) {
                        potential_name += node->to_plain_text();
                    }
                    // Trim
                    size_t s = potential_name.find_first_not_of(" \t\n");
                    size_t e = potential_name.find_last_not_of(" \t\n");
                    if (s != std::string::npos && e != std::string::npos) {
                        param.name = potential_name.substr(s, e - s + 1);
                    } else {
                        param.name = potential_name;
                    }
                    value_nodes.clear();
                    advance(); // skip =
                } else {
                    auto node = parse_node();
                    if (node) {
                        value_nodes.push_back(std::move(node));
                    } else {
                        // Handle nested templates/links that return nullptr at their close
                        if (check(TokenType::TemplateClose) || check(TokenType::Pipe)) {
                            break;
                        }
                        advance();
                    }
                }
            }

            param.value = std::move(value_nodes);
            tmpl->parameters.push_back(std::move(param));

            if (check(TokenType::Pipe)) {
                advance();
            }
        }
    }

    if (check(TokenType::TemplateClose)) {
        tmpl->location.end = current().location.end;
        advance();
    }

    return tmpl;
}

NodePtr Parser::parse_parameter() {
    auto param = std::make_unique<ParameterNode>();
    param->location.begin = current().location.begin;

    advance(); // skip {{{

    // Parse parameter name until | or }}}
    std::string name;
    while (!at_end() && !check(TokenType::Pipe) && !check(TokenType::ParameterClose)) {
        name += current().text;
        advance();
    }

    // Trim
    size_t start = name.find_first_not_of(" \t\n");
    size_t end = name.find_last_not_of(" \t\n");
    if (start != std::string::npos && end != std::string::npos) {
        param->name = name.substr(start, end - start + 1);
    } else {
        param->name = name;
    }

    // Parse default value if present
    if (check(TokenType::Pipe)) {
        advance(); // skip |

        while (!at_end() && !check(TokenType::ParameterClose)) {
            auto node = parse_node();
            if (node) {
                param->default_value.push_back(std::move(node));
            } else {
                if (check(TokenType::ParameterClose)) {
                    break;
                }
                advance();
            }
        }
    }

    if (check(TokenType::ParameterClose)) {
        param->location.end = current().location.end;
        advance();
    }

    return param;
}

NodePtr Parser::parse_link() {
    bool is_category = check(TokenType::Category);
    SourcePosition start = current().location.begin;

    advance(); // skip [[

    // Parse target until | or ]]
    std::string target;
    while (!at_end() && !check(TokenType::LinkSeparator) && !check(TokenType::LinkClose)) {
        target += current().text;
        advance();
    }

    // Handle category
    if (is_category || target.starts_with("Category:") || target.starts_with("category:") ||
        target.starts_with("Kategoria:") || target.starts_with("kategoria:")) {
        auto cat = std::make_unique<CategoryNode>();
        cat->location.begin = start;

        // Remove Category: prefix
        size_t colon = target.find(':');
        if (colon != std::string::npos) {
            cat->category = target.substr(colon + 1);
        } else {
            cat->category = target;
        }

        // Check for sort key
        if (check(TokenType::LinkSeparator)) {
            advance();
            std::string sort_key;
            while (!at_end() && !check(TokenType::LinkClose)) {
                sort_key += current().text;
                advance();
            }
            cat->sort_key = sort_key;
        }

        if (check(TokenType::LinkClose)) {
            cat->location.end = current().location.end;
            advance();
        }

        return cat;
    }

    // Regular link
    auto link = std::make_unique<LinkNode>();
    link->location.begin = start;

    // Check for anchor
    size_t hash = target.find('#');
    if (hash != std::string::npos) {
        link->anchor = target.substr(hash + 1);
        target = target.substr(0, hash);
    }

    link->target = target;

    // Parse display text if present
    if (check(TokenType::LinkSeparator)) {
        advance(); // skip |

        while (!at_end() && !check(TokenType::LinkClose)) {
            auto node = parse_node();
            if (node) {
                link->display_content.push_back(std::move(node));
            } else {
                if (check(TokenType::LinkClose)) {
                    break;
                }
                advance();
            }
        }
    }

    if (check(TokenType::LinkClose)) {
        link->location.end = current().location.end;
        advance();
    }

    return link;
}

NodePtr Parser::parse_external_link() {
    auto link = std::make_unique<ExternalLinkNode>();
    link->location.begin = current().location.begin;
    link->bracketed = true;

    advance(); // skip [

    // Parse URL (until space or ])
    std::string url;
    while (!at_end() && !check(TokenType::Whitespace) && !check(TokenType::ExternalLinkClose)) {
        url += current().text;
        advance();
    }
    link->url = url;

    // Skip whitespace
    if (check(TokenType::Whitespace)) {
        advance();
    }

    // Parse display text
    while (!at_end() && !check(TokenType::ExternalLinkClose)) {
        auto node = parse_node();
        if (node) {
            link->display_content.push_back(std::move(node));
        } else {
            break;
        }
    }

    if (check(TokenType::ExternalLinkClose)) {
        link->location.end = current().location.end;
        advance();
    }

    return link;
}

NodePtr Parser::parse_formatting() {
    auto fmt = std::make_unique<FormattingNode>();
    const Token &tok = current();

    fmt->location.begin = tok.location.begin;

    switch (tok.type) {
        case TokenType::Bold:
            fmt->style = FormattingNode::Style::Bold;
            break;
        case TokenType::Italic:
            fmt->style = FormattingNode::Style::Italic;
            break;
        case TokenType::BoldItalic:
            fmt->style = FormattingNode::Style::BoldItalic;
            break;
        default:
            break;
    }

    advance(); // skip opening '''

    // Parse content until matching closing marker or newline
    while (!at_end() && !check(TokenType::Newline)) {
        // Check for closing marker
        if ((fmt->style == FormattingNode::Style::Bold && check(TokenType::Bold)) ||
            (fmt->style == FormattingNode::Style::Italic && check(TokenType::Italic)) ||
            (fmt->style == FormattingNode::Style::BoldItalic && check(TokenType::BoldItalic))) {
            fmt->location.end = current().location.end;
            advance();
            break;
        }

        auto node = parse_node();
        if (node) {
            fmt->content.push_back(std::move(node));
        } else {
            break;
        }
    }

    return fmt;
}

NodePtr Parser::parse_html_tag() {
    auto tag = std::make_unique<HtmlTagNode>();
    const Token &tok = current();

    tag->location.begin = tok.location.begin;
    tag->tag_name = std::string(tok.tag_name);
    tag->self_closing = tok.self_closing;

    advance(); // skip opening tag

    if (!tag->self_closing) {
        // Parse content until closing tag
        while (!at_end() && !check(TokenType::HtmlTagClose)) {
            auto node = parse_node();
            if (node) {
                tag->content.push_back(std::move(node));
            } else {
                break;
            }
        }

        // Check if closing tag matches
        if (check(TokenType::HtmlTagClose)) {
            tag->location.end = current().location.end;
            advance();
        }
    }

    return tag;
}

// ============================================================================
// Helper methods
// ============================================================================

void Parser::advance() {
    if (tokenizer_) {
        tokenizer_->next();
    }
}

const Token &Parser::current() const {
    static Token eof_token{TokenType::EndOfInput, {}, {}};
    if (tokenizer_) {
        return tokenizer_->peek();
    }
    return eof_token;
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::at_end() const {
    return check(TokenType::EndOfInput);
}

void Parser::add_error(std::string message, ErrorSeverity severity) {
    ParseError err;
    err.message = std::move(message);
    err.location = current().location;
    err.severity = severity;
    errors_.push_back(std::move(err));
}

void Parser::synchronize() {
    // Skip tokens until we find a synchronization point
    while (!at_end()) {
        if (check(TokenType::Newline) || check(TokenType::Heading) || check(TokenType::TableStart) ||
            check(TokenType::BulletList) || check(TokenType::NumberedList)) {
            return;
        }
        advance();
    }
}

// ============================================================================
// Convenience functions
// ============================================================================

ParseResult parse(std::string_view input) {
    Parser parser;
    return parser.parse(input);
}

std::string parse_to_text(std::string_view input) {
    auto result = parse(input);
    if (result.document) {
        return result.document->to_plain_text();
    }
    return std::string(input);
}

NodeList parse_inline(std::string_view input) {
    Parser parser;
    return parser.parse_inline(input);
}

} // namespace wikilib::markup

