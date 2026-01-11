#pragma once

/**
 * @file types.hpp
 * @brief Fundamental type definitions for wikilib
 */

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace wikilib {

// ============================================================================
// Basic type aliases
// ============================================================================

using PageId = uint64_t;
using RevisionId = uint64_t;
using NamespaceId = int32_t;

// ============================================================================
// Source location tracking
// ============================================================================

/**
 * @brief Position in source text
 */
struct SourcePosition {
    uint32_t line = 1;
    uint32_t column = 1;
    size_t offset = 0;

    bool operator==(const SourcePosition &) const = default;
    auto operator<=>(const SourcePosition &) const = default;
};

/**
 * @brief Range in source text
 */
struct SourceRange {
    SourcePosition begin;
    SourcePosition end;

    [[nodiscard]] size_t length() const noexcept {
        return end.offset - begin.offset;
    }

    [[nodiscard]] bool empty() const noexcept {
        return begin.offset == end.offset;
    }

    [[nodiscard]] bool contains(const SourcePosition &pos) const noexcept {
        return pos.offset >= begin.offset && pos.offset < end.offset;
    }
};

// ============================================================================
// Error handling
// ============================================================================

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity {
    Warning, // Non-fatal issue, parsing continues
    Error, // Recoverable error
    Fatal // Unrecoverable error, parsing stops
};

/**
 * @brief Parse error information
 */
struct ParseError {
    std::string message;
    SourceRange location;
    ErrorSeverity severity = ErrorSeverity::Error;
    std::string context; // Surrounding text for error messages

    [[nodiscard]] std::string format() const;
};

/**
 * @brief Result type for operations that can fail
 */
template<typename T>
using Result = std::expected<T, ParseError>;

// ============================================================================
// Wiki-specific types
// ============================================================================

/**
 * @brief MediaWiki namespace definition
 */
struct Namespace {
    NamespaceId id;
    std::string name;
    std::string canonical_name;
    bool is_content = false;
    bool is_talk = false;
    bool allow_subpages = false;
};

/**
 * @brief Page metadata
 */
struct PageInfo {
    PageId id = 0;
    std::string title;
    NamespaceId namespace_id = 0;
    RevisionId revision_id = 0;
    std::string timestamp;
    std::optional<std::string> redirect_target;

    [[nodiscard]] bool is_redirect() const noexcept {
        return redirect_target.has_value();
    }

    [[nodiscard]] std::string full_title() const;
};

// ============================================================================
// Memory management helpers
// ============================================================================

/**
 * @brief Non-owning string view with source location
 */
struct LocatedString {
    std::string_view text;
    SourceRange location;

    [[nodiscard]] bool empty() const noexcept {
        return text.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        return text.size();
    }
};

} // namespace wikilib
