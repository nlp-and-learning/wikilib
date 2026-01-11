#pragma once

/**
 * @file unicode_utils.hpp
 * @brief Unicode handling utilities for UTF-8 strings
 */

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace wikilib::unicode {

// ============================================================================
// UTF-8 validation and properties
// ============================================================================

/**
 * @brief Check if string is valid UTF-8
 */
[[nodiscard]] bool is_valid_utf8(std::string_view str) noexcept;

/**
 * @brief Count UTF-8 code points (not bytes)
 */
[[nodiscard]] size_t count_codepoints(std::string_view str) noexcept;

/**
 * @brief Get byte length of UTF-8 character starting at pos
 * @return Number of bytes (1-4) or 0 if invalid
 */
[[nodiscard]] size_t utf8_char_length(char first_byte) noexcept;

/**
 * @brief Decode single UTF-8 code point
 * @param str Input string
 * @param pos Position to decode from (updated to next character)
 * @return Code point or nullopt if invalid
 */
[[nodiscard]] std::optional<char32_t> decode_utf8(std::string_view str, size_t &pos) noexcept;

/**
 * @brief Encode code point to UTF-8
 */
[[nodiscard]] std::string encode_utf8(char32_t codepoint);

// ============================================================================
// Case conversion (Unicode-aware)
// ============================================================================

/**
 * @brief Convert to lowercase (Unicode-aware)
 * @note Requires ICU for full Unicode support, falls back to ASCII otherwise
 */
[[nodiscard]] std::string to_lower(std::string_view str);

/**
 * @brief Convert to uppercase (Unicode-aware)
 */
[[nodiscard]] std::string to_upper(std::string_view str);

/**
 * @brief Convert to title case (first letter of each word uppercase)
 */
[[nodiscard]] std::string to_title_case(std::string_view str);

/**
 * @brief Capitalize first character only (MediaWiki style)
 */
[[nodiscard]] std::string capitalize_first(std::string_view str);

// ============================================================================
// Normalization
// ============================================================================

/**
 * @brief Unicode normalization forms
 */
enum class NormalizationForm {
    NFC, // Canonical Decomposition, followed by Canonical Composition
    NFD, // Canonical Decomposition
    NFKC, // Compatibility Decomposition, followed by Canonical Composition
    NFKD // Compatibility Decomposition
};

/**
 * @brief Normalize string to specified form
 * @note Requires ICU, returns input unchanged if not available
 */
[[nodiscard]] std::string normalize(std::string_view str, NormalizationForm form = NormalizationForm::NFC);

// ============================================================================
// Character classification
// ============================================================================

/**
 * @brief Check if code point is whitespace (Unicode-aware)
 */
[[nodiscard]] bool is_whitespace(char32_t cp) noexcept;

/**
 * @brief Check if code point is letter (Unicode-aware)
 */
[[nodiscard]] bool is_letter(char32_t cp) noexcept;

/**
 * @brief Check if code point is digit (Unicode-aware)
 */
[[nodiscard]] bool is_digit(char32_t cp) noexcept;

/**
 * @brief Check if code point is alphanumeric
 */
[[nodiscard]] bool is_alphanumeric(char32_t cp) noexcept;

/**
 * @brief Check if code point is punctuation
 */
[[nodiscard]] bool is_punctuation(char32_t cp) noexcept;

// ============================================================================
// MediaWiki-specific normalization
// ============================================================================

/**
 * @brief Normalize page title according to MediaWiki rules
 *
 * - Trims whitespace
 * - Replaces underscores with spaces
 * - Collapses multiple spaces
 * - Capitalizes first letter (unless in special namespace)
 * - Removes fragment (#section)
 */
[[nodiscard]] std::string normalize_title(std::string_view title);

/**
 * @brief Normalize for case-insensitive comparison
 *
 * MediaWiki titles are case-sensitive except for first character.
 * This creates a form suitable for comparison.
 */
[[nodiscard]] std::string normalize_for_comparison(std::string_view str);

/**
 * @brief Convert title to URL-safe format
 */
[[nodiscard]] std::string title_to_url(std::string_view title);

/**
 * @brief Convert URL-encoded title back to normal form
 */
[[nodiscard]] std::string url_to_title(std::string_view url);

// ============================================================================
// Iteration helpers
// ============================================================================

/**
 * @brief Iterator for UTF-8 code points
 */
class Utf8Iterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = char32_t;
    using difference_type = std::ptrdiff_t;
    using pointer = const char32_t *;
    using reference = char32_t;

    Utf8Iterator() = default;
    explicit Utf8Iterator(std::string_view str, size_t pos = 0);

    char32_t operator*() const;
    Utf8Iterator &operator++();
    Utf8Iterator operator++(int);

    bool operator==(const Utf8Iterator &other) const;
    bool operator!=(const Utf8Iterator &other) const;

    [[nodiscard]] size_t byte_position() const noexcept {
        return pos_;
    }
private:
    std::string_view str_;
    size_t pos_ = 0;
    char32_t current_ = 0;

    void decode_current();
    void advance();
};

/**
 * @brief Range adaptor for iterating UTF-8 code points
 */
class Utf8View {
public:
    explicit Utf8View(std::string_view str) : str_(str) {
    }

    [[nodiscard]] Utf8Iterator begin() const {
        return Utf8Iterator(str_);
    }

    [[nodiscard]] Utf8Iterator end() const {
        return Utf8Iterator(str_, str_.size());
    }
private:
    std::string_view str_;
};

/**
 * @brief Create UTF-8 view for range-based for loops
 */
[[nodiscard]] inline Utf8View utf8_codepoints(std::string_view str) {
    return Utf8View(str);
}

} // namespace wikilib::unicode
