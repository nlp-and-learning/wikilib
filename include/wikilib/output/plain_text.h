#pragma once

/**
 * @file plain_text.hpp
 * @brief Convert parsed wikitext to plain text
 */

#include <string>
#include <string_view>
#include "wikilib/markup/ast.h"

namespace wikilib::output {

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Options for plain text conversion
 */
struct PlainTextConfig {
    bool include_headings = true; // Keep section headings
    bool heading_underlines = false; // Add === under headings
    bool include_links = true; // Keep link text
    bool show_link_targets = false; // Show [link](target) format
    bool include_lists = true; // Convert lists to text
    bool list_bullets = true; // Use • for bullets
    bool include_tables = true; // Convert tables to text
    bool preserve_paragraphs = true; // Keep paragraph breaks
    bool collapse_whitespace = true; // Normalize whitespace
    int max_line_width = 0; // Word wrap (0 = no wrap)
    std::string paragraph_separator = "\n\n";
    std::string list_item_prefix = "• ";
    std::string numbered_item_format = "{}. "; // {} replaced with number
};

// ============================================================================
// Plain text converter
// ============================================================================

/**
 * @brief Convert parsed wikitext AST to plain text
 */
class PlainTextConverter {
public:
    explicit PlainTextConverter(PlainTextConfig config = {});

    /**
     * @brief Convert AST to plain text
     */
    [[nodiscard]] std::string convert(const markup::Node &node);

    /**
     * @brief Convert document to plain text
     */
    [[nodiscard]] std::string convert(const markup::DocumentNode &doc);

    /**
     * @brief Get/modify configuration
     */
    [[nodiscard]] const PlainTextConfig &config() const noexcept {
        return config_;
    }

    PlainTextConfig &config() noexcept {
        return config_;
    }
private:
    PlainTextConfig config_;

    void process_node(const markup::Node &node, std::string &output);
    void process_children(const markup::Node &node, std::string &output);
    void add_text(std::string_view text, std::string &output);
    void add_paragraph_break(std::string &output);
    void word_wrap(std::string &text);
};

// ============================================================================
// Convenience functions
// ============================================================================

/**
 * @brief Quick conversion from wikitext to plain text
 */
[[nodiscard]] std::string to_plain_text(std::string_view wikitext);

/**
 * @brief Convert with custom config
 */
[[nodiscard]] std::string to_plain_text(std::string_view wikitext, const PlainTextConfig &config);

/**
 * @brief Convert AST node to plain text
 */
[[nodiscard]] std::string to_plain_text(const markup::Node &node);

/**
 * @brief Extract just the first paragraph
 */
[[nodiscard]] std::string extract_first_paragraph(std::string_view wikitext);

/**
 * @brief Extract summary (first N characters/sentences)
 */
[[nodiscard]] std::string extract_summary(std::string_view wikitext, size_t max_chars = 500);

} // namespace wikilib::output
