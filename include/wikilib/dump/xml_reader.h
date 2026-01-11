#pragma once

/**
 * @file xml_reader.hpp
 * @brief Streaming XML reader for MediaWiki dump files
 */

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include "wikilib/core/types.h"
#include "wikilib/dump/bz2_stream.h"

namespace wikilib::dump {

// ============================================================================
// XML Events
// ============================================================================

/**
 * @brief XML event types
 */
enum class XmlEventType { StartDocument, EndDocument, StartElement, EndElement, Text, Comment, Error };

/**
 * @brief XML attribute
 */
struct XmlAttribute {
    std::string_view name;
    std::string_view value;
};

/**
 * @brief XML event data
 */
struct XmlEvent {
    XmlEventType type;
    std::string_view name; // Element/attribute name
    std::string_view text; // Text content
    std::vector<XmlAttribute> attributes;

    [[nodiscard]] std::optional<std::string_view> get_attribute(std::string_view name) const;
};

// ============================================================================
// Streaming XML reader
// ============================================================================

/**
 * @brief Streaming XML reader for large dump files
 *
 * Memory-efficient pull parser that doesn't load entire document.
 * Optimized for MediaWiki dump format.
 */
class XmlReader {
public:
    /**
     * @brief Create reader from file path
     */
    explicit XmlReader(const std::string &path);

    /**
     * @brief Create reader from BZ2 stream
     */
    explicit XmlReader(std::unique_ptr<Bz2Stream> stream);

    /**
     * @brief Create reader from string (for testing)
     */
    static XmlReader from_string(std::string_view xml);

    ~XmlReader();

    // Non-copyable, movable
    XmlReader(const XmlReader &) = delete;
    XmlReader &operator=(const XmlReader &) = delete;
    XmlReader(XmlReader &&) noexcept;
    XmlReader &operator=(XmlReader &&) noexcept;

    /**
     * @brief Read next event
     * @return Event or nullopt at end of document
     */
    [[nodiscard]] std::optional<XmlEvent> next();

    /**
     * @brief Skip current element and all its children
     */
    void skip_element();

    /**
     * @brief Read text content of current element
     */
    [[nodiscard]] std::string read_text();

    /**
     * @brief Check if at end of document
     */
    [[nodiscard]] bool eof() const noexcept;

    /**
     * @brief Get current element depth
     */
    [[nodiscard]] int depth() const noexcept;

    /**
     * @brief Get current element path (e.g., "mediawiki/page/revision")
     */
    [[nodiscard]] std::string current_path() const;

    /**
     * @brief Get bytes processed (for progress)
     */
    [[nodiscard]] uint64_t bytes_processed() const noexcept;

    /**
     * @brief Get any error message
     */
    [[nodiscard]] std::string_view error() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Higher-level iterators
// ============================================================================

/**
 * @brief Iterator over XML elements matching a path
 */
class XmlElementIterator {
public:
    XmlElementIterator(XmlReader &reader, std::string path);

    struct Element {
        std::string_view name;
        std::vector<XmlAttribute> attributes;
        std::string text_content;

        [[nodiscard]] std::optional<std::string_view> attribute(std::string_view name) const;
    };

    /**
     * @brief Get next matching element
     */
    [[nodiscard]] std::optional<Element> next();

    /**
     * @brief For range-based for loops
     */
    class Iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = Element;
        using difference_type = std::ptrdiff_t;
        using pointer = const Element *;
        using reference = const Element &;

        Iterator() = default;
        explicit Iterator(XmlElementIterator *iter);

        const Element &operator*() const {
            return current_;
        }

        Iterator &operator++();
        bool operator==(const Iterator &other) const;

        bool operator!=(const Iterator &other) const {
            return !(*this == other);
        }
    private:
        XmlElementIterator *iter_ = nullptr;
        Element current_;
        bool at_end_ = true;
    };

    [[nodiscard]] Iterator begin() {
        return Iterator(this);
    }

    [[nodiscard]] Iterator end() {
        return Iterator();
    }
private:
    XmlReader &reader_;
    std::string path_;
    std::vector<std::string> path_parts_;
};

} // namespace wikilib::dump
