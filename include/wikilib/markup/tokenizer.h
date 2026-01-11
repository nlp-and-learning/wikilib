#pragma once

/**
 * @file tokenizer.hpp
 * @brief Tokenizer for MediaWiki wikitext markup
 */

#include <string_view>
#include <variant>
#include <vector>
#include "wikilib/core/types.h"

namespace wikilib::markup {

// ============================================================================
// Token types
// ============================================================================

/**
 * @brief Types of tokens in MediaWiki markup
 */
enum class TokenType {
    // Text
    Text, // Plain text
    Whitespace, // Spaces, tabs (not newlines)
    Newline, // Line break

    // Formatting
    Bold, // '''
    Italic, // ''
    BoldItalic, // '''''

    // Links
    LinkOpen, // [[
    LinkClose, // ]]
    LinkSeparator, // | inside links
    ExternalLinkOpen, // [
    ExternalLinkClose, // ]

    // Templates
    TemplateOpen, // {{
    TemplateClose, // }}
    ParameterOpen, // {{{
    ParameterClose, // }}}
    Pipe, // | (parameter separator)
    Equals, // = (parameter name separator)

    // Tables
    TableStart, // {|
    TableEnd, // |}
    TableRowStart, // |-
    TableHeaderCell, // !
    TableDataCell, // |
    TableCaption, // |+

    // Headings
    Heading, // ==...== (level stored in token)

    // Lists
    BulletList, // *
    NumberedList, // #
    DefinitionTerm, // ;
    DefinitionDesc, // :
    Indent, // : (at line start, not in definition)

    // HTML-like
    HtmlTagOpen, // <tag
    HtmlTagClose, // </tag> or />
    HtmlComment, // <!-- ... -->
    NoWiki, // <nowiki>...</nowiki>

    // Special
    HorizontalRule, // ----
    MagicWord, // __TOC__, __NOTOC__, etc.
    Redirect, // #REDIRECT
    Category, // [[Category:...]]

    // Parser functions
    ParserFunction, // {{#if:...}}, {{#switch:...}}, etc.

    // End of input
    EndOfInput
};

/**
 * @brief Convert token type to string
 */
[[nodiscard]] std::string_view token_type_name(TokenType type) noexcept;

/**
 * @brief Single token from the tokenizer
 */
struct Token {
    TokenType type = TokenType::EndOfInput;
    std::string_view text; // Raw text of the token
    SourceRange location;

    // Extra data for certain token types
    int level = 0; // Heading level, list depth, etc.
    std::string_view tag_name; // For HTML tags
    bool self_closing = false; // For HTML tags

    [[nodiscard]] bool is(TokenType t) const noexcept {
        return type == t;
    }

    [[nodiscard]] bool is_text() const noexcept {
        return type == TokenType::Text || type == TokenType::Whitespace;
    }

    [[nodiscard]] bool is_formatting() const noexcept {
        return type == TokenType::Bold || type == TokenType::Italic || type == TokenType::BoldItalic;
    }

    [[nodiscard]] bool is_list_marker() const noexcept {
        return type == TokenType::BulletList || type == TokenType::NumberedList || type == TokenType::DefinitionTerm ||
               type == TokenType::DefinitionDesc;
    }
};

// ============================================================================
// Tokenizer configuration
// ============================================================================

/**
 * @brief Configuration options for tokenizer
 */
struct TokenizerConfig {
    bool preserve_comments = false; // Keep HTML comments as tokens
    bool preserve_nowiki = true; // Keep nowiki content
    bool recognize_redirects = true; // Recognize #REDIRECT
    bool recognize_categories = true; // Recognize [[Category:...]]
    bool lenient = true; // Continue on errors
};

// ============================================================================
// Tokenizer
// ============================================================================

/**
 * @brief Tokenizer for MediaWiki wikitext
 *
 * Converts raw wikitext into a stream of tokens. The tokenizer is stateful
 * and context-sensitive (e.g., | means different things in different contexts).
 */
class Tokenizer {
public:
    explicit Tokenizer(std::string_view input, TokenizerConfig config = {});

    /**
     * @brief Get next token
     */
    [[nodiscard]] Token next();

    /**
     * @brief Peek at next token without consuming
     */
    [[nodiscard]] const Token &peek();

    /**
     * @brief Peek at token n positions ahead (0 = next token)
     */
    [[nodiscard]] const Token &peek(size_t n);

    /**
     * @brief Check if there are more tokens
     */
    [[nodiscard]] bool has_more() const noexcept;

    /**
     * @brief Get current position in input
     */
    [[nodiscard]] SourcePosition position() const noexcept;

    /**
     * @brief Get any errors encountered
     */
    [[nodiscard]] const std::vector<ParseError> &errors() const noexcept;

    /**
     * @brief Reset tokenizer to beginning
     */
    void reset();

    /**
     * @brief Tokenize entire input at once
     */
    [[nodiscard]] std::vector<Token> tokenize_all();
private:
    std::string_view input_;
    TokenizerConfig config_;
    size_t pos_ = 0;
    SourcePosition current_pos_;
    std::vector<Token> lookahead_;
    std::vector<ParseError> errors_;

    // Context tracking
    int template_depth_ = 0;
    int link_depth_ = 0;
    bool at_line_start_ = true;
    bool in_table_ = false;

    Token scan_token();
    Token scan_text();
    Token scan_formatting();
    Token scan_link();
    Token scan_template();
    Token scan_table();
    Token scan_heading();
    Token scan_list_marker();
    Token scan_html_tag();
    Token scan_html_comment();
    Token scan_nowiki();
    Token scan_magic_word();

    void advance(size_t count = 1);
    [[nodiscard]] char current() const noexcept;
    [[nodiscard]] char peek_char(size_t offset = 1) const noexcept;
    [[nodiscard]] bool match(std::string_view str) const noexcept;
    [[nodiscard]] bool at_end() const noexcept;

    void skip_whitespace();
    void update_line_tracking();
    void add_error(std::string message, ErrorSeverity severity = ErrorSeverity::Warning);
};

// ============================================================================
// Utility functions
// ============================================================================

/**
 * @brief Quick check if text looks like wikitext (has markup)
 */
[[nodiscard]] bool looks_like_wikitext(std::string_view text) noexcept;

/**
 * @brief Extract just the text content from tokens (strip markup)
 */
[[nodiscard]] std::string tokens_to_plain_text(std::span<const Token> tokens);

/**
 * @brief Strip HTML comments from text (first pass preprocessing)
 *
 * Removes HTML comments (<!--...-->) while respecting nowiki tags.
 * Comments inside <nowiki>...</nowiki> are preserved.
 * The nowiki tags themselves are preserved in the output.
 *
 * @param input The wikitext to process
 * @return Text with comments removed, nowiki tags preserved
 */
[[nodiscard]] std::string strip_comments(std::string_view input);

/**
 * @brief Strip HTML comments, HTML tags, and unwrap nowiki tags from text
 *
 * Two-pass processing:
 * 1. First removes comments (respecting nowiki protection)
 * 2. Then tokenizes and strips HTML tags, unwraps nowiki content
 *
 * @param input The wikitext to process
 * @return Text with comments and tags stripped, nowiki unwrapped
 */
[[nodiscard]] std::string strip_comments_and_nowiki(std::string_view input);

} // namespace wikilib::markup
