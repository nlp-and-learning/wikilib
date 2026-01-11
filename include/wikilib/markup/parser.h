#pragma once

/**
 * @file parser.hpp
 * @brief Parser for MediaWiki wikitext markup
 */

#include <functional>
#include <memory>
#include "wikilib/core/types.h"
#include "wikilib/markup/ast.h"
#include "wikilib/markup/tokenizer.h"

namespace wikilib::markup {

// ============================================================================
// Parser configuration
// ============================================================================

/**
 * @brief Parser configuration options
 */
struct ParserConfig {
    TokenizerConfig tokenizer;

    bool expand_templates = false; // Expand templates during parsing
    bool preserve_whitespace = false; // Keep exact whitespace
    bool parse_tables = true; // Parse table markup
    bool parse_lists = true; // Parse list markup
    int max_depth = 100; // Maximum nesting depth
    int max_template_depth = 40; // Maximum template recursion
    bool lenient = true; // Continue on errors
};

// ============================================================================
// Parse result
// ============================================================================

/**
 * @brief Result of parsing operation
 */
struct ParseResult {
    std::unique_ptr<DocumentNode> document;
    std::vector<ParseError> errors;

    [[nodiscard]] bool success() const {
        return document != nullptr;
    }

    [[nodiscard]] bool has_errors() const {
        return !errors.empty();
    }

    [[nodiscard]] bool has_fatal_errors() const {
        for (const auto &e: errors) {
            if (e.severity == ErrorSeverity::Fatal)
                return true;
        }
        return false;
    }
};

// ============================================================================
// Parser
// ============================================================================

/**
 * @brief Parser for MediaWiki wikitext
 *
 * Converts wikitext into an Abstract Syntax Tree (AST).
 * The parser is context-sensitive and handles MediaWiki's complex grammar.
 */
class Parser {
public:
    explicit Parser(ParserConfig config = {});

    /**
     * @brief Parse wikitext into AST
     */
    [[nodiscard]] ParseResult parse(std::string_view input);

    /**
     * @brief Parse wikitext with page context
     */
    [[nodiscard]] ParseResult parse(std::string_view input, const PageInfo &page);

    /**
     * @brief Parse a fragment (inline content only)
     */
    [[nodiscard]] NodeList parse_inline(std::string_view input);

    /**
     * @brief Parse just template parameters
     */
    [[nodiscard]] std::vector<TemplateParameter> parse_template_params(std::string_view input);

    /**
     * @brief Get parser configuration
     */
    [[nodiscard]] const ParserConfig &config() const noexcept {
        return config_;
    }

    /**
     * @brief Modify parser configuration
     */
    ParserConfig &config() noexcept {
        return config_;
    }
private:
    ParserConfig config_;
    std::unique_ptr<Tokenizer> tokenizer_;
    std::vector<ParseError> errors_;
    int depth_ = 0;

    // Parsing methods
    NodeList parse_content();
    NodeList parse_block_content();
    NodeList parse_inline_content();

    NodePtr parse_node();
    NodePtr parse_paragraph();
    NodePtr parse_heading();
    NodePtr parse_list();
    NodePtr parse_table();
    NodePtr parse_template();
    NodePtr parse_parameter();
    NodePtr parse_link();
    NodePtr parse_external_link();
    NodePtr parse_formatting();
    NodePtr parse_html_tag();

    // Table parsing
    NodePtr parse_table_row();
    NodePtr parse_table_cell();

    // List parsing
    NodePtr parse_list_item();

    // Helper methods
    void advance();
    [[nodiscard]] const Token &current() const;
    [[nodiscard]] bool check(TokenType type) const;
    [[nodiscard]] bool match(TokenType type);
    [[nodiscard]] bool at_end() const;

    void add_error(std::string message, ErrorSeverity severity = ErrorSeverity::Warning);
    void synchronize(); // Error recovery

    // Depth tracking
    class DepthGuard {
    public:
        explicit DepthGuard(Parser &p) : parser_(p) {
            ++parser_.depth_;
        }

        ~DepthGuard() {
            --parser_.depth_;
        }

        [[nodiscard]] bool exceeded() const {
            return parser_.depth_ > parser_.config_.max_depth;
        }
    private:
        Parser &parser_;
    };
};

// ============================================================================
// Convenience functions
// ============================================================================

/**
 * @brief Parse wikitext with default settings
 */
[[nodiscard]] ParseResult parse(std::string_view input);

/**
 * @brief Parse and extract plain text
 */
[[nodiscard]] std::string parse_to_text(std::string_view input);

/**
 * @brief Quick parse for specific content type
 */
[[nodiscard]] NodeList parse_inline(std::string_view input);

} // namespace wikilib::markup
