/**
 * @file json_writer.cpp
 * @brief Implementation of JSON output for parsed wikitext and dump data
 */

#include "wikilib/output/json_writer.h"
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "wikilib/markup/parser.h"

namespace wikilib::output {

// ============================================================================
// JsonWriter implementation
// ============================================================================

JsonWriter::JsonWriter(JsonConfig config) : config_(std::move(config)) {
}

JsonWriter::JsonWriter(std::ostream &output, JsonConfig config) : config_(std::move(config)), output_(&output) {
}

std::string JsonWriter::write(const markup::Node &node) {
    std::ostringstream oss;
    write_to(node, oss);
    return oss.str();
}

std::string JsonWriter::write(const markup::DocumentNode &doc) {
    return write(static_cast<const markup::Node &>(doc));
}

std::string JsonWriter::write(const dump::Page &page) {
    std::ostringstream oss;
    write_to(page, oss);
    return oss.str();
}

void JsonWriter::write_to(const markup::Node &node, std::ostream &out) {
    depth_ = 0;
    write_node(node, out);
}

void JsonWriter::write_to(const dump::Page &page, std::ostream &out) {
    depth_ = 0;

    out << '{';
    write_newline(out);
    depth_++;

    // Page info
    write_indent(out);
    out << "\"id\": " << page.info.id << ',';
    write_newline(out);

    write_indent(out);
    out << "\"title\": ";
    write_value(page.info.title, out);
    out << ',';
    write_newline(out);

    write_indent(out);
    out << "\"namespace\": " << page.info.namespace_id << ',';
    write_newline(out);

    write_indent(out);
    out << "\"redirect\": ";
    if (page.info.redirect_target.has_value()) {
        write_value(*page.info.redirect_target, out);
    } else {
        out << "null";
    }
    out << ',';
    write_newline(out);

    // Revisions
    write_indent(out);
    out << "\"revisions\": [";
    write_newline(out);
    depth_++;

    bool first_rev = true;
    for (const auto &rev: page.revisions) {
        if (!first_rev) {
            out << ',';
            write_newline(out);
        }
        first_rev = false;

        write_indent(out);
        out << '{';
        write_newline(out);
        depth_++;

        write_indent(out);
        out << "\"id\": " << rev.id << ',';
        write_newline(out);

        if (rev.parent_id != 0) {
            write_indent(out);
            out << "\"parent_id\": " << rev.parent_id << ',';
            write_newline(out);
        }

        write_indent(out);
        out << "\"timestamp\": ";
        write_value(rev.timestamp, out);
        out << ',';
        write_newline(out);

        write_indent(out);
        out << "\"contributor\": ";
        write_value(rev.contributor, out);
        out << ',';
        write_newline(out);

        if (!rev.comment.empty()) {
            write_indent(out);
            out << "\"comment\": ";
            write_value(rev.comment, out);
            out << ',';
            write_newline(out);
        }

        write_indent(out);
        out << "\"model\": ";
        write_value(rev.model, out);
        out << ',';
        write_newline(out);

        write_indent(out);
        out << "\"format\": ";
        write_value(rev.format, out);
        out << ',';
        write_newline(out);

        if (config_.include_raw_text) {
            write_indent(out);
            out << "\"content\": ";
            write_value(rev.content, out);
            out << ',';
            write_newline(out);
        }

        if (!rev.sha1.empty()) {
            write_indent(out);
            out << "\"sha1\": ";
            write_value(rev.sha1, out);
        }

        write_newline(out);
        depth_--;
        write_indent(out);
        out << '}';
    }

    write_newline(out);
    depth_--;
    write_indent(out);
    out << ']';
    write_newline(out);

    depth_--;
    out << '}';
}

void JsonWriter::begin_array() {
    if (output_) {
        *output_ << '[';
        write_newline(*output_);
        depth_++;
        first_item_ = true;
    }
}

void JsonWriter::write_array_item(const dump::Page &page) {
    if (output_) {
        if (!first_item_) {
            *output_ << ',';
            write_newline(*output_);
        }
        first_item_ = false;

        write_indent(*output_);
        write_to(page, *output_);
    }
}

void JsonWriter::write_array_item(const markup::DocumentNode &doc) {
    if (output_) {
        if (!first_item_) {
            *output_ << ',';
            write_newline(*output_);
        }
        first_item_ = false;

        write_indent(*output_);
        write_to(doc, *output_);
    }
}

void JsonWriter::end_array() {
    if (output_) {
        write_newline(*output_);
        depth_--;
        write_indent(*output_);
        *output_ << ']';
    }
}

void JsonWriter::write_node(const markup::Node &node, std::ostream &out) {
    using namespace markup;

    out << '{';
    write_newline(out);
    depth_++;

    // Type field
    write_indent(out);
    out << "\"type\": ";
    write_value(node_type_name(node.type), out);

    // Source location (optional)
    if (config_.include_source_locations) {
        out << ',';
        write_newline(out);
        write_indent(out);
        out << "\"location\": {";
        write_newline(out);
        depth_++;
        write_indent(out);
        out << "\"start\": " << node.location.begin.offset << ',';
        write_newline(out);
        write_indent(out);
        out << "\"end\": " << node.location.end.offset;
        write_newline(out);
        depth_--;
        write_indent(out);
        out << '}';
    }

    // Node-specific fields
    switch (node.type) {
        case NodeType::Text: {
            const auto &text_node = static_cast<const TextNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"text\": ";
            write_value(text_node.text, out);
            break;
        }

        case NodeType::Formatting: {
            const auto &fmt = static_cast<const FormattingNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"style\": ";
            switch (fmt.style) {
                case FormattingNode::Style::Bold:
                    write_value("bold", out);
                    break;
                case FormattingNode::Style::Italic:
                    write_value("italic", out);
                    break;
                case FormattingNode::Style::BoldItalic:
                    write_value("bold_italic", out);
                    break;
            }
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"content\": ";
            write_children(fmt.content, out);
            break;
        }

        case NodeType::Link: {
            const auto &link = static_cast<const LinkNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"target\": ";
            write_value(link.target, out);
            if (!link.anchor.empty()) {
                out << ',';
                write_newline(out);
                write_indent(out);
                out << "\"anchor\": ";
                write_value(link.anchor, out);
            }
            if (!link.display_content.empty()) {
                out << ',';
                write_newline(out);
                write_indent(out);
                out << "\"display\": ";
                write_children(link.display_content, out);
            }
            break;
        }

        case NodeType::ExternalLink: {
            const auto &link = static_cast<const ExternalLinkNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"url\": ";
            write_value(link.url, out);
            if (!link.display_content.empty()) {
                out << ',';
                write_newline(out);
                write_indent(out);
                out << "\"display\": ";
                write_children(link.display_content, out);
            }
            break;
        }

        case NodeType::Template: {
            const auto &tmpl = static_cast<const TemplateNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"name\": ";
            write_value(tmpl.name, out);
            if (!tmpl.parameters.empty()) {
                out << ',';
                write_newline(out);
                write_indent(out);
                out << "\"parameters\": [";
                write_newline(out);
                depth_++;

                bool first = true;
                for (const auto &param: tmpl.parameters) {
                    if (!first) {
                        out << ',';
                        write_newline(out);
                    }
                    first = false;

                    write_indent(out);
                    out << '{';
                    write_newline(out);
                    depth_++;

                    write_indent(out);
                    out << "\"name\": ";
                    if (param.name.has_value()) {
                        write_value(*param.name, out);
                    } else {
                        out << "null";
                    }
                    out << ',';
                    write_newline(out);

                    write_indent(out);
                    out << "\"value\": ";
                    write_children(param.value, out);

                    write_newline(out);
                    depth_--;
                    write_indent(out);
                    out << '}';
                }

                write_newline(out);
                depth_--;
                write_indent(out);
                out << ']';
            }
            break;
        }

        case NodeType::Parameter: {
            const auto &param = static_cast<const ParameterNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"name\": ";
            write_value(param.name, out);
            if (!param.default_value.empty()) {
                out << ',';
                write_newline(out);
                write_indent(out);
                out << "\"default\": ";
                write_children(param.default_value, out);
            }
            break;
        }

        case NodeType::Heading: {
            const auto &heading = static_cast<const HeadingNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"level\": " << heading.level << ',';
            write_newline(out);
            write_indent(out);
            out << "\"content\": ";
            write_children(heading.content, out);
            break;
        }

        case NodeType::List: {
            const auto &list = static_cast<const ListNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"list_type\": ";
            switch (list.list_type) {
                case ListNode::ListType::Bullet:
                    write_value("bullet", out);
                    break;
                case ListNode::ListType::Numbered:
                    write_value("numbered", out);
                    break;
                case ListNode::ListType::Definition:
                    write_value("definition", out);
                    break;
            }
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"items\": ";
            write_children(list.items, out);
            break;
        }

        case NodeType::ListItem: {
            const auto &item = static_cast<const ListItemNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"depth\": " << item.depth << ',';
            write_newline(out);
            write_indent(out);
            out << "\"is_definition_term\": " << (item.is_definition_term ? "true" : "false") << ',';
            write_newline(out);
            write_indent(out);
            out << "\"content\": ";
            write_children(item.content, out);
            break;
        }

        case NodeType::Table: {
            const auto &table = static_cast<const TableNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"attributes\": ";
            write_value(table.attributes, out);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"rows\": ";
            write_children(table.rows, out);
            if (table.caption.has_value()) {
                out << ',';
                write_newline(out);
                write_indent(out);
                out << "\"caption\": ";
                write_value(*table.caption, out);
            }
            break;
        }

        case NodeType::TableRow: {
            const auto &row = static_cast<const TableRowNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"attributes\": ";
            write_value(row.attributes, out);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"cells\": ";
            write_children(row.cells, out);
            break;
        }

        case NodeType::TableCell: {
            const auto &cell = static_cast<const TableCellNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"is_header\": " << (cell.is_header ? "true" : "false") << ',';
            write_newline(out);
            write_indent(out);
            out << "\"attributes\": ";
            write_value(cell.attributes, out);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"content\": ";
            write_children(cell.content, out);
            break;
        }

        case NodeType::HtmlTag: {
            const auto &tag = static_cast<const HtmlTagNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"tag_name\": ";
            write_value(tag.tag_name, out);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"self_closing\": " << (tag.self_closing ? "true" : "false");
            if (!tag.attributes.empty()) {
                out << ',';
                write_newline(out);
                write_indent(out);
                out << "\"attributes\": {";
                write_newline(out);
                depth_++;

                bool first = true;
                for (const auto &[key, value]: tag.attributes) {
                    if (!first) {
                        out << ',';
                        write_newline(out);
                    }
                    first = false;

                    write_indent(out);
                    write_value(key, out);
                    out << ": ";
                    write_value(value, out);
                }

                write_newline(out);
                depth_--;
                write_indent(out);
                out << '}';
            }
            if (!tag.content.empty()) {
                out << ',';
                write_newline(out);
                write_indent(out);
                out << "\"content\": ";
                write_children(tag.content, out);
            }
            break;
        }

        case NodeType::Comment: {
            const auto &comment = static_cast<const CommentNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"content\": ";
            write_value(comment.content, out);
            break;
        }

        case NodeType::NoWiki: {
            const auto &nowiki = static_cast<const NoWikiNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"content\": ";
            write_value(nowiki.content, out);
            break;
        }

        case NodeType::Category: {
            const auto &cat = static_cast<const CategoryNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"category\": ";
            write_value(cat.category, out);
            if (!cat.sort_key.empty()) {
                out << ',';
                write_newline(out);
                write_indent(out);
                out << "\"sort_key\": ";
                write_value(cat.sort_key, out);
            }
            break;
        }

        case NodeType::Redirect: {
            const auto &redirect = static_cast<const RedirectNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"target\": ";
            write_value(redirect.target, out);
            break;
        }

        case NodeType::MagicWord: {
            const auto &magic = static_cast<const MagicWordNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"word\": ";
            write_value(magic.word, out);
            break;
        }

        case NodeType::Paragraph: {
            const auto &para = static_cast<const ParagraphNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"content\": ";
            write_children(para.content, out);
            break;
        }

        case NodeType::HorizontalRule: {
            // No additional fields
            break;
        }

        case NodeType::Document: {
            const auto &doc = static_cast<const DocumentNode &>(node);
            out << ',';
            write_newline(out);
            write_indent(out);
            out << "\"content\": ";
            write_children(doc.content, out);
            break;
        }

        default:
            // Unknown node type - include children if any
            if (!node.children().empty()) {
                out << ',';
                write_newline(out);
                write_indent(out);
                out << "\"children\": ";
                write_children(node.children(), out);
            }
            break;
    }

    write_newline(out);
    depth_--;
    write_indent(out);
    out << '}';
}

void JsonWriter::write_value(std::string_view str, std::ostream &out) {
    out << '"' << escape_string(str) << '"';
}

void JsonWriter::write_indent(std::ostream &out) {
    if (config_.pretty_print) {
        for (int i = 0; i < depth_ * config_.indent_size; ++i) {
            out << ' ';
        }
    }
}

void JsonWriter::write_newline(std::ostream &out) {
    if (config_.pretty_print) {
        out << '\n';
    }
}

std::string JsonWriter::escape_string(std::string_view str) {
    std::string result;
    result.reserve(str.size() + str.size() / 8); // Allow for some escaping

    for (size_t i = 0; i < str.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(str[i]);

        switch (c) {
            case '"':
                result += "\\\"";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\b':
                result += "\\b";
                break;
            case '\f':
                result += "\\f";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                if (c < 0x20) {
                    // Control character - escape as \u00XX
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    result += buf;
                } else if (config_.escape_unicode && c > 0x7F) {
                    // Non-ASCII character - escape as \uXXXX
                    // Handle UTF-8 sequences
                    uint32_t codepoint = 0;
                    int remaining = 0;

                    if ((c & 0xE0) == 0xC0) {
                        codepoint = c & 0x1F;
                        remaining = 1;
                    } else if ((c & 0xF0) == 0xE0) {
                        codepoint = c & 0x0F;
                        remaining = 2;
                    } else if ((c & 0xF8) == 0xF0) {
                        codepoint = c & 0x07;
                        remaining = 3;
                    } else {
                        // Invalid UTF-8 start byte, just pass through
                        result += static_cast<char>(c);
                        continue;
                    }

                    bool valid = true;
                    for (int j = 0; j < remaining && i + j + 1 < str.size(); ++j) {
                        unsigned char next = static_cast<unsigned char>(str[i + j + 1]);
                        if ((next & 0xC0) != 0x80) {
                            valid = false;
                            break;
                        }
                        codepoint = (codepoint << 6) | (next & 0x3F);
                    }

                    if (valid) {
                        i += remaining;
                        if (codepoint <= 0xFFFF) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", codepoint);
                            result += buf;
                        } else {
                            // Surrogate pair for codepoints > 0xFFFF
                            codepoint -= 0x10000;
                            uint16_t high = 0xD800 + ((codepoint >> 10) & 0x3FF);
                            uint16_t low = 0xDC00 + (codepoint & 0x3FF);
                            char buf[16];
                            snprintf(buf, sizeof(buf), "\\u%04x\\u%04x", high, low);
                            result += buf;
                        }
                    } else {
                        // Invalid UTF-8, pass through
                        result += static_cast<char>(c);
                    }
                } else {
                    result += static_cast<char>(c);
                }
                break;
        }
    }

    return result;
}

// Private helper methods for writing children
void JsonWriter::write_children(const std::vector<std::unique_ptr<markup::Node>> &children, std::ostream &out) {
    out << '[';
    write_newline(out);
    depth_++;

    bool first = true;
    for (const auto &child: children) {
        if (!child)
            continue;

        if (!first) {
            out << ',';
            write_newline(out);
        }
        first = false;

        write_indent(out);
        write_node(*child, out);
    }

    write_newline(out);
    depth_--;
    write_indent(out);
    out << ']';
}

// ============================================================================
// JsonLinesWriter implementation
// ============================================================================

JsonLinesWriter::JsonLinesWriter(std::ostream &output) : output_(&output) {
}

JsonLinesWriter::JsonLinesWriter(const std::string &path) :
    owned_output_(std::make_unique<std::ofstream>(path)), output_(owned_output_.get()) {
}

JsonLinesWriter::~JsonLinesWriter() {
    flush();
}

void JsonLinesWriter::write(const dump::Page &page) {
    if (!output_)
        return;

    JsonConfig config;
    config.pretty_print = false; // JSONL requires single-line JSON

    JsonWriter writer(config);
    *output_ << writer.write(page) << '\n';
    ++count_;
}

void JsonLinesWriter::write(const dump::Page &page, TransformFunc transform) {
    if (!output_ || !transform)
        return;

    std::string json = transform(page);
    *output_ << json << '\n';
    ++count_;
}

void JsonLinesWriter::flush() {
    if (output_) {
        output_->flush();
    }
}

// ============================================================================
// Convenience functions
// ============================================================================

std::string to_json(const markup::Node &node) {
    JsonWriter writer;
    return writer.write(node);
}

std::string to_json(const dump::Page &page) {
    JsonWriter writer;
    return writer.write(page);
}

std::string wikitext_to_json(std::string_view wikitext) {
    auto result = markup::parse(wikitext);
    if (!result.document) {
        return "{}";
    }

    JsonWriter writer;
    return writer.write(*result.document);
}

} // namespace wikilib::output
