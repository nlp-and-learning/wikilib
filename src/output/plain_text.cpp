/**
 * @file plain_text.cpp
 * @brief Implementation of plain text conversion from wikitext
 */

#include "wikilib/output/plain_text.h"
#include <algorithm>
#include <sstream>
#include "wikilib/markup/ast.h"
#include "wikilib/markup/parser.h"

namespace wikilib::output {

// ============================================================================
// PlainTextConverter implementation
// ============================================================================

PlainTextConverter::PlainTextConverter(PlainTextConfig config) : config_(std::move(config)) {
}

std::string PlainTextConverter::convert(const markup::Node &node) {
    std::string output;
    process_node(node, output);

    if (config_.collapse_whitespace) {
        // Normalize multiple spaces to single space
        std::string normalized;
        normalized.reserve(output.size());
        bool last_was_space = false;
        bool last_was_newline = false;

        for (char c: output) {
            if (c == '\n') {
                if (!last_was_newline) {
                    normalized += '\n';
                    last_was_newline = true;
                }
                last_was_space = true;
            } else if (c == ' ' || c == '\t') {
                if (!last_was_space) {
                    normalized += ' ';
                    last_was_space = true;
                }
            } else {
                normalized += c;
                last_was_space = false;
                last_was_newline = false;
            }
        }

        output = std::move(normalized);
    }

    // Trim leading/trailing whitespace
    size_t start = output.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = output.find_last_not_of(" \t\n\r");
    output = output.substr(start, end - start + 1);

    if (config_.max_line_width > 0) {
        word_wrap(output);
    }

    return output;
}

std::string PlainTextConverter::convert(const markup::DocumentNode &doc) {
    return convert(static_cast<const markup::Node &>(doc));
}

void PlainTextConverter::process_node(const markup::Node &node, std::string &output) {
    using namespace markup;

    switch (node.type) {
        case NodeType::Text: {
            const auto &text_node = static_cast<const TextNode &>(node);
            add_text(text_node.text, output);
            break;
        }

        case NodeType::Formatting: {
            // Just include the content, ignore formatting markers
            process_children(node, output);
            break;
        }

        case NodeType::Link: {
            const auto &link = static_cast<const LinkNode &>(node);
            if (config_.include_links) {
                if (config_.show_link_targets) {
                    output += '[';
                    if (link.has_custom_display()) {
                        process_children(node, output);
                    } else {
                        output += link.target;
                    }
                    output += "](";
                    output += link.full_target();
                    output += ')';
                } else {
                    if (link.has_custom_display()) {
                        process_children(node, output);
                    } else {
                        output += link.target;
                    }
                }
            }
            break;
        }

        case NodeType::ExternalLink: {
            const auto &link = static_cast<const ExternalLinkNode &>(node);
            if (config_.include_links) {
                if (!link.display_content.empty()) {
                    process_children(node, output);
                } else if (config_.show_link_targets) {
                    output += link.url;
                }
            }
            break;
        }

        case NodeType::Template:
        case NodeType::Parameter:
            // Skip templates - they would need expansion
            break;

        case NodeType::Heading: {
            const auto &heading = static_cast<const HeadingNode &>(node);
            if (config_.include_headings) {
                add_paragraph_break(output);

                process_children(node, output);

                if (config_.heading_underlines) {
                    output += '\n';
                    char underline_char = (heading.level <= 2) ? '=' : '-';
                    // Get length of heading text
                    std::string heading_text;
                    for (const auto &child: heading.content) {
                        heading_text += child->to_plain_text();
                    }
                    output += std::string(heading_text.size(), underline_char);
                }

                add_paragraph_break(output);
            }
            break;
        }

        case NodeType::List: {
            if (config_.include_lists) {
                add_paragraph_break(output);
                process_children(node, output);
            }
            break;
        }

        case NodeType::ListItem: {
            const auto &item = static_cast<const ListItemNode &>(node);
            if (config_.include_lists) {
                // Indentation based on depth
                for (int i = 1; i < item.depth; ++i) {
                    output += "  ";
                }

                if (config_.list_bullets) {
                    output += config_.list_item_prefix;
                }

                process_children(node, output);
                output += '\n';
            }
            break;
        }

        case NodeType::Table: {
            if (config_.include_tables) {
                add_paragraph_break(output);
                process_children(node, output);
                add_paragraph_break(output);
            }
            break;
        }

        case NodeType::TableRow: {
            if (config_.include_tables) {
                process_children(node, output);
                output += '\n';
            }
            break;
        }

        case NodeType::TableCell: {
            if (config_.include_tables) {
                process_children(node, output);
                output += '\t';
            }
            break;
        }

        case NodeType::HtmlTag: {
            const auto &tag = static_cast<const HtmlTagNode &>(node);
            // Handle some specific tags
            if (tag.tag_name == "br") {
                output += '\n';
            } else if (tag.tag_name == "p") {
                add_paragraph_break(output);
                process_children(node, output);
                add_paragraph_break(output);
            } else if (tag.tag_name == "ref" || tag.tag_name == "references") {
                // Skip references
            } else {
                // Include content of other tags
                process_children(node, output);
            }
            break;
        }

        case NodeType::Paragraph: {
            if (config_.preserve_paragraphs) {
                process_children(node, output);
                add_paragraph_break(output);
            } else {
                process_children(node, output);
                output += ' ';
            }
            break;
        }

        case NodeType::Comment:
            // Skip comments
            break;

        case NodeType::NoWiki: {
            const auto &nowiki = static_cast<const NoWikiNode &>(node);
            add_text(nowiki.content, output);
            break;
        }

        case NodeType::Redirect:
        case NodeType::Category:
        case NodeType::MagicWord:
            // Skip these special elements
            break;

        case NodeType::HorizontalRule: {
            add_paragraph_break(output);
            if (config_.max_line_width > 0) {
                output += std::string(config_.max_line_width, '-');
            } else {
                output += "---";
            }
            add_paragraph_break(output);
            break;
        }

        case NodeType::Document: {
            process_children(node, output);
            break;
        }

        default:
            // For unknown nodes, try to process children
            process_children(node, output);
            break;
    }
}

void PlainTextConverter::process_children(const markup::Node &node, std::string &output) {
    for (const auto &child: node.children()) {
        if (child) {
            process_node(*child, output);
        }
    }
}

void PlainTextConverter::add_text(std::string_view text, std::string &output) {
    output += text;
}

void PlainTextConverter::add_paragraph_break(std::string &output) {
    if (output.empty())
        return;

    // Don't add multiple paragraph breaks
    size_t end = output.size();
    while (end > 0 && (output[end - 1] == ' ' || output[end - 1] == '\t')) {
        --end;
    }

    // Check if we already have a paragraph break
    if (end >= config_.paragraph_separator.size()) {
        bool has_break = true;
        for (size_t i = 0; i < config_.paragraph_separator.size(); ++i) {
            if (output[end - config_.paragraph_separator.size() + i] != config_.paragraph_separator[i]) {
                has_break = false;
                break;
            }
        }
        if (has_break)
            return;
    }

    output += config_.paragraph_separator;
}

void PlainTextConverter::word_wrap(std::string &text) {
    if (config_.max_line_width <= 0)
        return;

    std::string result;
    result.reserve(text.size() + text.size() / config_.max_line_width);

    size_t line_start = 0;
    size_t last_space = 0;
    size_t col = 0;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        if (c == '\n') {
            result += text.substr(line_start, i - line_start + 1);
            line_start = i + 1;
            last_space = line_start;
            col = 0;
        } else if (c == ' ') {
            last_space = i;
            ++col;
        } else {
            ++col;
        }

        if (col >= static_cast<size_t>(config_.max_line_width) && last_space > line_start) {
            result += text.substr(line_start, last_space - line_start);
            result += '\n';
            line_start = last_space + 1;
            col = i - last_space;
            last_space = line_start;
        }
    }

    // Add remaining text
    if (line_start < text.size()) {
        result += text.substr(line_start);
    }

    text = std::move(result);
}

// ============================================================================
// Convenience functions
// ============================================================================

std::string to_plain_text(std::string_view wikitext) {
    return to_plain_text(wikitext, PlainTextConfig{});
}

std::string to_plain_text(std::string_view wikitext, const PlainTextConfig &config) {
    auto result = markup::parse(wikitext);
    if (!result.document) {
        return std::string(wikitext);
    }

    PlainTextConverter converter(config);
    return converter.convert(*result.document);
}

std::string to_plain_text(const markup::Node &node) {
    PlainTextConverter converter;
    return converter.convert(node);
}

std::string extract_first_paragraph(std::string_view wikitext) {
    auto result = markup::parse(wikitext);
    if (!result.document) {
        return "";
    }

    // Find first paragraph or text content
    std::string output;

    for (const auto &node: result.document->content) {
        if (!node)
            continue;

        if (node->type == markup::NodeType::Paragraph) {
            PlainTextConverter converter;
            return converter.convert(*node);
        }

        if (node->type == markup::NodeType::Text) {
            const auto &text_node = static_cast<const markup::TextNode &>(*node);
            output += text_node.text;
        }

        // Stop at first heading or significant break
        if (node->type == markup::NodeType::Heading || node->type == markup::NodeType::HorizontalRule ||
            node->type == markup::NodeType::Table) {
            break;
        }
    }

    // Trim
    size_t start = output.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return "";
    size_t end = output.find_last_not_of(" \t\n\r");
    return output.substr(start, end - start + 1);
}

std::string extract_summary(std::string_view wikitext, size_t max_chars) {
    std::string full_text = to_plain_text(wikitext);

    if (full_text.size() <= max_chars) {
        return full_text;
    }

    // Try to cut at sentence boundary
    size_t cut_point = max_chars;

    // Look for sentence end
    for (size_t i = max_chars; i > max_chars / 2; --i) {
        if (full_text[i] == '.' || full_text[i] == '!' || full_text[i] == '?') {
            // Check for space or end after punctuation
            if (i + 1 >= full_text.size() || full_text[i + 1] == ' ' || full_text[i + 1] == '\n') {
                cut_point = i + 1;
                break;
            }
        }
    }

    // If no sentence boundary, try word boundary
    if (cut_point == max_chars) {
        for (size_t i = max_chars; i > max_chars / 2; --i) {
            if (full_text[i] == ' ') {
                cut_point = i;
                break;
            }
        }
    }

    std::string summary = full_text.substr(0, cut_point);

    // Trim trailing whitespace
    size_t end = summary.find_last_not_of(" \t\n\r");
    if (end != std::string::npos) {
        summary = summary.substr(0, end + 1);
    }

    // Add ellipsis if we cut mid-text
    if (cut_point < full_text.size() && !summary.empty()) {
        char last = summary.back();
        if (last != '.' && last != '!' && last != '?') {
            summary += "...";
        }
    }

    return summary;
}

} // namespace wikilib::output
