#pragma once

/**
 * @file text_utils.hpp
 * @brief Common text manipulation utilities
 */

#include <algorithm>
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Collection of text manipulation utilities
 *
 * Provides common operations needed for MediaWiki markup processing.
 * All functions are designed to work with UTF-8 encoded strings.
 */
namespace wikilib::text {

// ============================================================================
// Trimming
// ============================================================================

/**
 * @brief Remove leading whitespace
 */
[[nodiscard]] std::string_view trim_left(std::string_view str) noexcept;

/**
 * @brief Remove trailing whitespace
 */
[[nodiscard]] std::string_view trim_right(std::string_view str) noexcept;

/**
 * @brief Remove leading and trailing whitespace
 */
[[nodiscard]] std::string_view trim(std::string_view str) noexcept;

/**
 * @brief Collapse multiple consecutive whitespace to single space
 */
[[nodiscard]] std::string collapse_whitespace(std::string_view str);

// ============================================================================
// Case conversion (ASCII only, fast path)
// ============================================================================

/**
 * @brief Convert ASCII characters to lowercase (in-place)
 */
void to_lower_ascii(std::string &str) noexcept;

/**
 * @brief Convert ASCII characters to uppercase (in-place)
 */
void to_upper_ascii(std::string &str) noexcept;

/**
 * @brief Return lowercase copy (ASCII only)
 */
[[nodiscard]] std::string to_lower_ascii_copy(std::string_view str);

/**
 * @brief Return uppercase copy (ASCII only)
 */
[[nodiscard]] std::string to_upper_ascii_copy(std::string_view str);

// ============================================================================
// Comparison
// ============================================================================

/**
 * @brief Case-insensitive ASCII comparison
 */
[[nodiscard]] bool equals_ignore_case_ascii(std::string_view a, std::string_view b) noexcept;

/**
 * @brief Check if string starts with prefix
 */
[[nodiscard]] constexpr bool starts_with(std::string_view str, std::string_view prefix) noexcept {
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

/**
 * @brief Check if string ends with suffix
 */
[[nodiscard]] constexpr bool ends_with(std::string_view str, std::string_view suffix) noexcept {
    return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
}

/**
 * @brief Case-insensitive starts_with (ASCII)
 */
[[nodiscard]] bool starts_with_ignore_case_ascii(std::string_view str, std::string_view prefix) noexcept;

// ============================================================================
// Splitting
// ============================================================================

/**
 * @brief Split string by delimiter
 */
[[nodiscard]] std::vector<std::string_view> split(std::string_view str, char delimiter);

/**
 * @brief Split string by string delimiter
 */
[[nodiscard]] std::vector<std::string_view> split(std::string_view str, std::string_view delimiter);

/**
 * @brief Split into at most n parts
 */
[[nodiscard]] std::vector<std::string_view> split_n(std::string_view str, char delimiter, size_t max_parts);

/**
 * @brief Split by any character in delimiters set
 */
[[nodiscard]] std::vector<std::string_view> split_any(std::string_view str, std::string_view delimiters);

// ============================================================================
// Joining
// ============================================================================

/**
 * @brief Join strings with separator
 */
template<typename Container>
[[nodiscard]] std::string join(const Container &parts, std::string_view separator) {
    std::string result;
    bool first = true;
    for (const auto &part: parts) {
        if (!first) {
            result += separator;
        }
        result += part;
        first = false;
    }
    return result;
}

// ============================================================================
// Search and replace
// ============================================================================

/**
 * @brief Replace all occurrences of 'from' with 'to'
 */
[[nodiscard]] std::string replace_all(std::string_view str, std::string_view from, std::string_view to);

/**
 * @brief Replace first occurrence of 'from' with 'to'
 */
[[nodiscard]] std::string replace_first(std::string_view str, std::string_view from, std::string_view to);

/**
 * @brief Count occurrences of substring
 */
[[nodiscard]] size_t count_occurrences(std::string_view str, std::string_view substr) noexcept;

/**
 * @brief Find all positions of substring
 */
[[nodiscard]] std::vector<size_t> find_all(std::string_view str, std::string_view substr);

// ============================================================================
// Number parsing
// ============================================================================

/**
 * @brief Parse integer from string
 */
template<typename T>
[[nodiscard]] std::optional<T> parse_int(std::string_view str) noexcept {
    str = trim(str);
    if (str.empty())
        return std::nullopt;

    T value{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec == std::errc{} && ptr == str.data() + str.size()) {
        return value;
    }
    return std::nullopt;
}

// ============================================================================
// HTML/XML entity handling
// ============================================================================

/**
 * @brief Decode HTML entities (&amp; -> &, etc.)
 */
[[nodiscard]] std::string decode_html_entities(std::string_view str);

/**
 * @brief Encode special characters as HTML entities
 */
[[nodiscard]] std::string encode_html_entities(std::string_view str);

/**
 * @brief Strip HTML/XML tags
 */
[[nodiscard]] std::string strip_tags(std::string_view str);

// ============================================================================
// Line handling
// ============================================================================

/**
 * @brief Split into lines (handles \n, \r\n, \r)
 */
[[nodiscard]] std::vector<std::string_view> split_lines(std::string_view str);

/**
 * @brief Count number of lines
 */
[[nodiscard]] size_t count_lines(std::string_view str) noexcept;

/**
 * @brief Get specific line (0-indexed)
 */
[[nodiscard]] std::optional<std::string_view> get_line(std::string_view str, size_t line_number);

} // namespace wikilib::text
