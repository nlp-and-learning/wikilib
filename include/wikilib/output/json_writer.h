#pragma once

/**
 * @file json_writer.hpp
 * @brief JSON output for parsed wikitext and dump data
 */

#include <functional>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include "wikilib/dump/page_handler.h"
#include "wikilib/markup/ast.h"

namespace wikilib::output {

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Options for JSON output
 */
struct JsonConfig {
    bool pretty_print = true; // Indented output
    int indent_size = 2; // Spaces per indent level
    bool include_source_locations = false; // Add location info to nodes
    bool include_raw_text = false; // Include original wikitext
    bool compact_text_nodes = true; // Merge adjacent text nodes
    bool escape_unicode = false; // Use \uXXXX escapes
};

// ============================================================================
// JSON Writer
// ============================================================================

/**
 * @brief Serialize wikitext AST and page data to JSON
 */
class JsonWriter {
public:
    explicit JsonWriter(JsonConfig config = {});
    explicit JsonWriter(std::ostream &output, JsonConfig config = {});

    /**
     * @brief Serialize AST node to JSON string
     */
    [[nodiscard]] std::string write(const markup::Node &node);

    /**
     * @brief Serialize document to JSON string
     */
    [[nodiscard]] std::string write(const markup::DocumentNode &doc);

    /**
     * @brief Serialize page to JSON string
     */
    [[nodiscard]] std::string write(const dump::Page &page);

    /**
     * @brief Write to output stream
     */
    void write_to(const markup::Node &node, std::ostream &out);
    void write_to(const dump::Page &page, std::ostream &out);

    /**
     * @brief Start JSON array (for streaming multiple items)
     */
    void begin_array();

    /**
     * @brief Add item to array
     */
    void write_array_item(const dump::Page &page);
    void write_array_item(const markup::DocumentNode &doc);

    /**
     * @brief End JSON array
     */
    void end_array();

    /**
     * @brief Get/modify configuration
     */
    [[nodiscard]] const JsonConfig &config() const noexcept {
        return config_;
    }

    JsonConfig &config() noexcept {
        return config_;
    }
private:
    JsonConfig config_;
    std::ostream *output_ = nullptr;
    int depth_ = 0;
    bool first_item_ = true;

    void write_node(const markup::Node &node, std::ostream &out);
    void write_value(std::string_view str, std::ostream &out);
    void write_indent(std::ostream &out);
    void write_newline(std::ostream &out);
    std::string escape_string(std::string_view str);
    void write_children(const std::vector<std::unique_ptr<markup::Node>> &children, std::ostream &out);
};

// ============================================================================
// Convenience functions
// ============================================================================

/**
 * @brief Convert AST to JSON string
 */
[[nodiscard]] std::string to_json(const markup::Node &node);

/**
 * @brief Convert page to JSON string
 */
[[nodiscard]] std::string to_json(const dump::Page &page);

/**
 * @brief Convert wikitext to JSON AST
 */
[[nodiscard]] std::string wikitext_to_json(std::string_view wikitext);

// ============================================================================
// JSONL (JSON Lines) output
// ============================================================================

/**
 * @brief Write pages as JSON Lines (one JSON object per line)
 */
class JsonLinesWriter {
public:
    explicit JsonLinesWriter(std::ostream &output);
    explicit JsonLinesWriter(const std::string &path);

    ~JsonLinesWriter();

    /**
     * @brief Write single page
     */
    void write(const dump::Page &page);

    /**
     * @brief Write page with custom transformation
     */
    using TransformFunc = std::function<std::string(const dump::Page &)>;
    void write(const dump::Page &page, TransformFunc transform);

    /**
     * @brief Flush output
     */
    void flush();

    /**
     * @brief Get number of items written
     */
    [[nodiscard]] size_t count() const noexcept {
        return count_;
    }
private:
    std::ostream *output_;
    std::unique_ptr<std::ostream> owned_output_;
    size_t count_ = 0;
};

} // namespace wikilib::output
