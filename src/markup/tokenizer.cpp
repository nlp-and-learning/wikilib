/**
 * @file tokenizer.cpp
 * @brief Implementation of MediaWiki wikitext tokenizer
 */

#include "wikilib/markup/tokenizer.h"
#include <algorithm>
#include <array>
#include <cctype>
#include "wikilib/core/types.h"

namespace wikilib::markup {

// ============================================================================
// Token type names
// ============================================================================

[[nodiscard]] std::string_view token_type_name(TokenType type) noexcept {
    switch (type) {
        case TokenType::Text:
            return "Text";
        case TokenType::Whitespace:
            return "Whitespace";
        case TokenType::Newline:
            return "Newline";
        case TokenType::Bold:
            return "Bold";
        case TokenType::Italic:
            return "Italic";
        case TokenType::BoldItalic:
            return "BoldItalic";
        case TokenType::LinkOpen:
            return "LinkOpen";
        case TokenType::LinkClose:
            return "LinkClose";
        case TokenType::LinkSeparator:
            return "LinkSeparator";
        case TokenType::ExternalLinkOpen:
            return "ExternalLinkOpen";
        case TokenType::ExternalLinkClose:
            return "ExternalLinkClose";
        case TokenType::TemplateOpen:
            return "TemplateOpen";
        case TokenType::TemplateClose:
            return "TemplateClose";
        case TokenType::ParameterOpen:
            return "ParameterOpen";
        case TokenType::ParameterClose:
            return "ParameterClose";
        case TokenType::Pipe:
            return "Pipe";
        case TokenType::Equals:
            return "Equals";
        case TokenType::TableStart:
            return "TableStart";
        case TokenType::TableEnd:
            return "TableEnd";
        case TokenType::TableRowStart:
            return "TableRowStart";
        case TokenType::TableHeaderCell:
            return "TableHeaderCell";
        case TokenType::TableDataCell:
            return "TableDataCell";
        case TokenType::TableCaption:
            return "TableCaption";
        case TokenType::Heading:
            return "Heading";
        case TokenType::BulletList:
            return "BulletList";
        case TokenType::NumberedList:
            return "NumberedList";
        case TokenType::DefinitionTerm:
            return "DefinitionTerm";
        case TokenType::DefinitionDesc:
            return "DefinitionDesc";
        case TokenType::Indent:
            return "Indent";
        case TokenType::HtmlTagOpen:
            return "HtmlTagOpen";
        case TokenType::HtmlTagClose:
            return "HtmlTagClose";
        case TokenType::HtmlComment:
            return "HtmlComment";
        case TokenType::NoWiki:
            return "NoWiki";
        case TokenType::HorizontalRule:
            return "HorizontalRule";
        case TokenType::MagicWord:
            return "MagicWord";
        case TokenType::Redirect:
            return "Redirect";
        case TokenType::Category:
            return "Category";
        case TokenType::ParserFunction:
            return "ParserFunction";
        case TokenType::EndOfInput:
            return "EndOfInput";
    }
    return "Unknown";
}

// ============================================================================
// Helper functions
// ============================================================================

/**
 * @brief Check if a tag name is a valid HTML tag allowed in MediaWiki
 *
 * Based on tags from MediaWiki's Sanitizer.php and common HTML tags
 * used in wikis. Does not include "nowiki" which is handled specially.
 */
static bool is_valid_html_tag(std::string_view tag_name) noexcept {
    // List of allowed HTML tags in MediaWiki (from TagFactory.h)
    // Sorted alphabetically for easier maintenance
    static constexpr std::array<std::string_view, 63> valid_tags = {
        "abbr", "b", "bdi", "bdo", "big", "blockquote", "br", "caption", "center",
        "cite", "code", "col", "colgroup", "data", "dd", "del", "dfn", "div", "dl",
        "dt", "em", "font", "h1", "h2", "h3", "h4", "h5", "h6", "hr", "i", "ins",
        "kbd", "li", "link", "mark", "meta", "ol", "p", "pre", "q", "rb", "rp",
        "rt", "ruby", "s", "samp", "small", "span", "strike", "strong", "sub",
        "sup", "table", "td", "th", "time", "tr", "tt", "u", "ul", "var", "wbr"
    };

    // Binary search since array is sorted
    return std::binary_search(valid_tags.begin(), valid_tags.end(), tag_name);
}

// ============================================================================
// Tokenizer implementation
// ============================================================================

Tokenizer::Tokenizer(std::string_view input, TokenizerConfig config) :
    input_(input), config_(config), pos_(0), current_pos_{1, 1, 0}, at_line_start_(true) {
}

Token Tokenizer::next() {
    if (!lookahead_.empty()) {
        Token tok = lookahead_.front();
        lookahead_.erase(lookahead_.begin());
        return tok;
    }
    return scan_token();
}

const Token &Tokenizer::peek() {
    return peek(0);
}

const Token &Tokenizer::peek(size_t n) {
    while (lookahead_.size() <= n) {
        lookahead_.push_back(scan_token());
    }
    return lookahead_[n];
}

bool Tokenizer::has_more() const noexcept {
    if (!lookahead_.empty()) {
        return lookahead_.front().type != TokenType::EndOfInput;
    }
    return pos_ < input_.size();
}

SourcePosition Tokenizer::position() const noexcept {
    return current_pos_;
}

const std::vector<ParseError> &Tokenizer::errors() const noexcept {
    return errors_;
}

void Tokenizer::reset() {
    pos_ = 0;
    current_pos_ = {1, 1, 0};
    lookahead_.clear();
    errors_.clear();
    template_depth_ = 0;
    link_depth_ = 0;
    at_line_start_ = true;
    in_table_ = false;
}

std::vector<Token> Tokenizer::tokenize_all() {
    std::vector<Token> tokens;
    while (has_more()) {
        Token tok = next();
        tokens.push_back(tok);
        if (tok.type == TokenType::EndOfInput) {
            break;
        }
    }
    return tokens;
}

// ============================================================================
// Private scanning methods
// ============================================================================

Token Tokenizer::scan_token() {
    if (at_end()) {
        return Token{TokenType::EndOfInput, {}, {current_pos_, current_pos_}};
    }

    SourcePosition start = current_pos_;
    char c = current();

    // Newline handling
    if (c == '\n') {
        advance();
        at_line_start_ = true;
        return Token{TokenType::Newline, input_.substr(start.offset, 1), {start, current_pos_}};
    }

    // Line-start constructs
    if (at_line_start_) {
        at_line_start_ = false;

        // Horizontal rule: ---- at line start
        if (c == '-' && match("----")) {
            size_t begin = pos_;
            while (!at_end() && current() == '-') {
                advance();
            }
            return Token{TokenType::HorizontalRule, input_.substr(begin, pos_ - begin), {start, current_pos_}};
        }

        // Heading: == at line start
        if (c == '=') {
            return scan_heading();
        }

        // List markers
        if (c == '*' || c == '#' || c == ';' || c == ':') {
            return scan_list_marker();
        }

        // Table start: {|
        if (c == '{' && peek_char() == '|') {
            advance(2);
            in_table_ = true;
            return Token{TokenType::TableStart, input_.substr(start.offset, 2), {start, current_pos_}};
        }

        // Table row/end at line start
        if (c == '|') {
            if (peek_char() == '}') {
                advance(2);
                in_table_ = false;
                return Token{TokenType::TableEnd, input_.substr(start.offset, 2), {start, current_pos_}};
            }
            if (peek_char() == '-') {
                advance(2);
                return Token{TokenType::TableRowStart, input_.substr(start.offset, 2), {start, current_pos_}};
            }
            if (peek_char() == '+') {
                advance(2);
                return Token{TokenType::TableCaption, input_.substr(start.offset, 2), {start, current_pos_}};
            }
            if (in_table_) {
                advance();
                return Token{TokenType::TableDataCell, input_.substr(start.offset, 1), {start, current_pos_}};
            }
        }

        // Table header cell: ! at line start
        if (c == '!' && in_table_) {
            advance();
            return Token{TokenType::TableHeaderCell, input_.substr(start.offset, 1), {start, current_pos_}};
        }
    }

    at_line_start_ = false;

    // Heading close: = in middle of line (for closing headings)
    // Scan = as heading if:
    // - We have at least one =
    // - We're not inside templates (where = is parameter separator)
    if (c == '=' && template_depth_ == 0) {
        return scan_heading();
    }

    // Apostrophes for formatting
    if (c == '\'') {
        return scan_formatting();
    }

    // Links: [[ or [
    if (c == '[') {
        return scan_link();
    }

    // Link close: ]] or ]
    if (c == ']') {
        if (peek_char() == ']' && link_depth_ > 0) {
            advance(2);
            link_depth_--;
            return Token{TokenType::LinkClose, input_.substr(start.offset, 2), {start, current_pos_}};
        }
        advance();
        return Token{TokenType::ExternalLinkClose, input_.substr(start.offset, 1), {start, current_pos_}};
    }

    // Templates and parameters: {{{ or {{
    if (c == '{') {
        return scan_template();
    }

    // Template/parameter close: }}} or }}
    if (c == '}') {
        if (match("}}}") && template_depth_ > 0) {
            advance(3);
            template_depth_--;
            return Token{TokenType::ParameterClose, input_.substr(start.offset, 3), {start, current_pos_}};
        }
        if (match("}}") && template_depth_ > 0) {
            advance(2);
            template_depth_--;
            return Token{TokenType::TemplateClose, input_.substr(start.offset, 2), {start, current_pos_}};
        }
    }

    // Pipe (context-sensitive)
    if (c == '|') {
        advance();
        if (link_depth_ > 0) {
            return Token{TokenType::LinkSeparator, input_.substr(start.offset, 1), {start, current_pos_}};
        }
        if (template_depth_ > 0 || in_table_) {
            return Token{TokenType::Pipe, input_.substr(start.offset, 1), {start, current_pos_}};
        }
        // Outside special context, treat as text
        return Token{TokenType::Text, input_.substr(start.offset, 1), {start, current_pos_}};
    }

    // Equals (in template context)
    if (c == '=' && template_depth_ > 0) {
        advance();
        return Token{TokenType::Equals, input_.substr(start.offset, 1), {start, current_pos_}};
    }

    // HTML comment: <!--
    if (c == '<' && match("<!--")) {
        return scan_html_comment();
    }

    // Nowiki: <nowiki>
    if (c == '<' && (match("<nowiki>") || match("<nowiki/>"))) {
        return scan_nowiki();
    }

    // HTML tags: <tag or </tag
    if (c == '<') {
        return scan_html_tag();
    }

    // Magic words: __WORD__
    if (c == '_' && match("__")) {
        return scan_magic_word();
    }

    // Redirect at start
    if (c == '#' && config_.recognize_redirects) {
        if (match("#REDIRECT") || match("#redirect") || match("#Redirect")) {
            size_t begin = pos_;
            advance(9); // #REDIRECT
            return Token{TokenType::Redirect, input_.substr(begin, 9), {start, current_pos_}};
        }
    }

    // Default: scan as text
    return scan_text();
}

Token Tokenizer::scan_text() {
    SourcePosition start = current_pos_;
    size_t begin = pos_;

    while (!at_end()) {
        char c = current();

        // Stop at special characters
        if (c == '\n' || c == '[' || c == ']' || c == '{' || c == '}' || c == '|' || c == '\'' || c == '<' ||
            c == '=' || c == '_' || c == '*' || c == '#' || c == ';' || c == ':' || c == '!' || c == '-') {
            break;
        }

        advance();
    }

    if (pos_ == begin) {
        // Single character that didn't match anything
        advance();
    }

    return Token{TokenType::Text, input_.substr(begin, pos_ - begin), {start, current_pos_}};
}

Token Tokenizer::scan_formatting() {
    SourcePosition start = current_pos_;
    size_t begin = pos_;
    int count = 0;

    while (!at_end() && current() == '\'' && count < 5) {
        advance();
        count++;
    }

    TokenType type;
    if (count >= 5) {
        type = TokenType::BoldItalic;
    } else if (count >= 3) {
        type = TokenType::Bold;
        // If we consumed more than 3, put back the extras
        if (count == 4) {
            // 4 apostrophes: treat as italic + text apostrophe + italic?
            // Or bold + one extra. Let's return bold and leave one.
            // Actually, for simplicity, return 3 as bold
            // Adjust position back by 1
            pos_--;
            current_pos_.column--;
            current_pos_.offset--;
        }
    } else if (count >= 2) {
        type = TokenType::Italic;
    } else {
        // Single apostrophe is just text
        return Token{TokenType::Text, input_.substr(begin, 1), {start, current_pos_}};
    }

    return Token{type, input_.substr(begin, pos_ - begin), {start, current_pos_}};
}

Token Tokenizer::scan_link() {
    SourcePosition start = current_pos_;

    if (match("[[")) {
        advance(2);
        link_depth_++;

        // Check for category
        if (config_.recognize_categories) {
            // Look for Category: prefix
            std::string_view rest = input_.substr(pos_);
            if (rest.starts_with("Category:") || rest.starts_with("category:") || rest.starts_with("Kategoria:") ||
                rest.starts_with("kategoria:")) {
                return Token{TokenType::Category, input_.substr(start.offset, 2), {start, current_pos_}};
            }
        }

        return Token{TokenType::LinkOpen, input_.substr(start.offset, 2), {start, current_pos_}};
    }

    // Single [ for external link
    advance();
    return Token{TokenType::ExternalLinkOpen, input_.substr(start.offset, 1), {start, current_pos_}};
}

Token Tokenizer::scan_template() {
    SourcePosition start = current_pos_;

    // Parameter: {{{
    if (match("{{{")) {
        advance(3);
        template_depth_++;
        return Token{TokenType::ParameterOpen, input_.substr(start.offset, 3), {start, current_pos_}};
    }

    // Template: {{
    if (match("{{")) {
        advance(2);
        template_depth_++;

        // Check for parser function: {{#
        if (!at_end() && current() == '#') {
            return Token{TokenType::ParserFunction, input_.substr(start.offset, 2), {start, current_pos_}};
        }

        return Token{TokenType::TemplateOpen, input_.substr(start.offset, 2), {start, current_pos_}};
    }

    // Single { is just text
    advance();
    return Token{TokenType::Text, input_.substr(start.offset, 1), {start, current_pos_}};
}

Token Tokenizer::scan_table() {
    SourcePosition start = current_pos_;

    if (match("{|")) {
        advance(2);
        in_table_ = true;
        return Token{TokenType::TableStart, input_.substr(start.offset, 2), {start, current_pos_}};
    }

    if (match("|}")) {
        advance(2);
        in_table_ = false;
        return Token{TokenType::TableEnd, input_.substr(start.offset, 2), {start, current_pos_}};
    }

    if (match("|-")) {
        advance(2);
        return Token{TokenType::TableRowStart, input_.substr(start.offset, 2), {start, current_pos_}};
    }

    if (match("|+")) {
        advance(2);
        return Token{TokenType::TableCaption, input_.substr(start.offset, 2), {start, current_pos_}};
    }

    advance();
    return Token{TokenType::TableDataCell, input_.substr(start.offset, 1), {start, current_pos_}};
}

Token Tokenizer::scan_heading() {
    SourcePosition start = current_pos_;
    size_t begin = pos_;
    int level = 0;

    // Count opening ='s
    while (!at_end() && current() == '=' && level < 6) {
        advance();
        level++;
    }

    Token tok;
    tok.type = TokenType::Heading;
    tok.text = input_.substr(begin, pos_ - begin);
    tok.location = {start, current_pos_};
    tok.level = level;

    return tok;
}

Token Tokenizer::scan_list_marker() {
    SourcePosition start = current_pos_;
    size_t begin = pos_;
    char marker = current();
    int depth = 0;

    // Count consecutive markers of the same type (or mixed for nested lists)
    while (!at_end() && (current() == '*' || current() == '#' || current() == ';' || current() == ':')) {
        advance();
        depth++;
    }

    TokenType type;
    switch (marker) {
        case '*':
            type = TokenType::BulletList;
            break;
        case '#':
            type = TokenType::NumberedList;
            break;
        case ';':
            type = TokenType::DefinitionTerm;
            break;
        case ':':
            type = TokenType::DefinitionDesc;
            break;
        default:
            type = TokenType::Indent;
            break;
    }

    Token tok;
    tok.type = type;
    tok.text = input_.substr(begin, pos_ - begin);
    tok.location = {start, current_pos_};
    tok.level = depth;

    return tok;
}

Token Tokenizer::scan_html_tag() {
    SourcePosition start = current_pos_;
    size_t begin = pos_;

    advance(); // skip <

    bool is_closing = false;
    if (!at_end() && current() == '/') {
        is_closing = true;
        advance();
    }

    // Read tag name
    size_t name_start = pos_;
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(current())) || current() == '-' || current() == '_')) {
        advance();
    }
    std::string_view tag_name = input_.substr(name_start, pos_ - name_start);

    // Check if tag name is valid
    // If not, treat '<' as plain text and don't consume the rest
    if (!is_valid_html_tag(tag_name)) {
        // Reset position to just after '<'
        pos_ = begin + 1;
        current_pos_.column = start.column + 1;
        current_pos_.offset = pos_;

        // Return '<' as text
        return Token{TokenType::Text, input_.substr(begin, 1), {start, current_pos_}};
    }

    // Skip to end of tag
    bool self_closing = false;
    while (!at_end() && current() != '>') {
        if (current() == '/' && peek_char() == '>') {
            self_closing = true;
            advance();
        }
        advance();
    }

    if (!at_end()) {
        advance(); // skip >
    }

    Token tok;
    tok.type = is_closing ? TokenType::HtmlTagClose : TokenType::HtmlTagOpen;
    tok.text = input_.substr(begin, pos_ - begin);
    tok.location = {start, current_pos_};
    tok.tag_name = tag_name;
    tok.self_closing = self_closing;

    return tok;
}

Token Tokenizer::scan_html_comment() {
    SourcePosition start = current_pos_;
    size_t begin = pos_;

    advance(4); // skip <!--

    // Find -->
    while (!at_end()) {
        if (match("-->")) {
            advance(3);
            break;
        }
        advance();
    }

    Token tok;
    tok.type = TokenType::HtmlComment;
    tok.text = input_.substr(begin, pos_ - begin);
    tok.location = {start, current_pos_};

    if (!config_.preserve_comments) {
        // Return next token instead if not preserving
        return scan_token();
    }

    return tok;
}

Token Tokenizer::scan_nowiki() {
    SourcePosition start = current_pos_;

    // Check for self-closing <nowiki/>
    if (match("<nowiki/>")) {
        advance(9);
        Token tok;
        tok.type = TokenType::NoWiki;
        tok.text = ""; // No content in self-closing tag
        tok.location = {start, current_pos_};
        tok.self_closing = true;
        return tok;
    }

    advance(8); // skip <nowiki>

    // Content starts here
    size_t content_begin = pos_;

    // Find </nowiki>
    while (!at_end()) {
        if (match("</nowiki>")) {
            break;
        }
        advance();
    }

    // Content ends here (before </nowiki>)
    size_t content_end = pos_;

    if (match("</nowiki>")) {
        advance(9);
    }

    Token tok;
    tok.type = TokenType::NoWiki;
    tok.text = input_.substr(content_begin, content_end - content_begin);
    tok.location = {start, current_pos_};

    return tok;
}

Token Tokenizer::scan_magic_word() {
    SourcePosition start = current_pos_;
    size_t begin = pos_;

    advance(2); // skip __

    // Read word
    while (!at_end() && (std::isalnum(static_cast<unsigned char>(current())) || current() == '_')) {
        advance();
    }

    // Check for closing __
    if (match("__")) {
        advance(2);
        return Token{TokenType::MagicWord, input_.substr(begin, pos_ - begin), {start, current_pos_}};
    }

    // Not a valid magic word, return as text
    return Token{TokenType::Text, input_.substr(begin, pos_ - begin), {start, current_pos_}};
}

// ============================================================================
// Helper methods
// ============================================================================

void Tokenizer::advance(size_t count) {
    for (size_t i = 0; i < count && pos_ < input_.size(); ++i) {
        if (input_[pos_] == '\n') {
            current_pos_.line++;
            current_pos_.column = 1;
        } else {
            current_pos_.column++;
        }
        pos_++;
        current_pos_.offset = pos_;
    }
}

char Tokenizer::current() const noexcept {
    if (pos_ >= input_.size()) {
        return '\0';
    }
    return input_[pos_];
}

char Tokenizer::peek_char(size_t offset) const noexcept {
    size_t idx = pos_ + offset;
    if (idx >= input_.size()) {
        return '\0';
    }
    return input_[idx];
}

bool Tokenizer::match(std::string_view str) const noexcept {
    if (pos_ + str.size() > input_.size()) {
        return false;
    }
    return input_.substr(pos_, str.size()) == str;
}

bool Tokenizer::at_end() const noexcept {
    return pos_ >= input_.size();
}

void Tokenizer::skip_whitespace() {
    while (!at_end() && (current() == ' ' || current() == '\t')) {
        advance();
    }
}

void Tokenizer::update_line_tracking() {
    // Called after newline to reset line-start state
    at_line_start_ = true;
}

void Tokenizer::add_error(std::string message, ErrorSeverity severity) {
    ParseError err;
    err.message = std::move(message);
    err.location = {current_pos_, current_pos_};
    err.severity = severity;

    // Extract some context
    size_t ctx_start = (pos_ > 20) ? pos_ - 20 : 0;
    size_t ctx_end = std::min(pos_ + 20, input_.size());
    err.context = std::string(input_.substr(ctx_start, ctx_end - ctx_start));

    errors_.push_back(std::move(err));
}

// ============================================================================
// Utility functions
// ============================================================================

bool looks_like_wikitext(std::string_view text) noexcept {
    // Quick heuristics to detect wikitext markup
    static constexpr std::array<std::string_view, 12> markers = {"[[", "]]", "{{", "}}",   "'''",    "''",
                                                                 "{|", "|}", "==", "<ref", "</ref>", "#REDIRECT"};

    for (const auto &marker: markers) {
        if (text.find(marker) != std::string_view::npos) {
            return true;
        }
    }

    return false;
}

std::string tokens_to_plain_text(std::span<const Token> tokens) {
    std::string result;
    result.reserve(tokens.size() * 10); // rough estimate

    for (const auto &tok: tokens) {
        switch (tok.type) {
            case TokenType::Text:
            case TokenType::Whitespace:
                result += tok.text;
                break;
            case TokenType::Newline:
                result += '\n';
                break;
            // Skip all markup tokens
            default:
                break;
        }
    }

    return result;
}

std::string remove_comments(std::string_view input) {
    Tokenizer tok(input, {.preserve_comments = true});
    auto tokens = tok.tokenize_all();

    std::string result;
    result.reserve(input.size());

    for (const auto& t : tokens) {
        if (t.type == TokenType::EndOfInput) break;
        if (t.type == TokenType::HtmlComment) continue;  // Comments are stripped
        if (t.type == TokenType::NoWiki) {
            result += t.text;  // NoWiki content is literal
        } else {
            result += std::string(t.text);
        }
    }
    return result;
}

} // namespace wikilib::markup
