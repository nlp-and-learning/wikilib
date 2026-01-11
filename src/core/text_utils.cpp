/**
 * @file text_utils.cpp
 * @brief Implementation of text manipulation utilities
 */

#include "wikilib/core/text_utils.hpp"
#include "wikilib/core/types.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace wikilib::text {

// ============================================================================
// Trimming
// ============================================================================

std::string_view trim_left(std::string_view str) noexcept {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    return str.substr(start);
}

std::string_view trim_right(std::string_view str) noexcept {
    size_t end = str.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return str.substr(0, end);
}

std::string_view trim(std::string_view str) noexcept {
    return trim_right(trim_left(str));
}

std::string collapse_whitespace(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    bool last_was_space = false;
    for (char c: str) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!last_was_space) {
                result += ' ';
                last_was_space = true;
            }
        } else {
            result += c;
            last_was_space = false;
        }
    }

    return result;
}

// ============================================================================
// Case conversion
// ============================================================================

void to_lower_ascii(std::string &str) noexcept {
    for (char &c: str) {
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 'a';
        }
    }
}

void to_upper_ascii(std::string &str) noexcept {
    for (char &c: str) {
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
    }
}

std::string to_lower_ascii_copy(std::string_view str) {
    std::string result(str);
    to_lower_ascii(result);
    return result;
}

std::string to_upper_ascii_copy(std::string_view str) {
    std::string result(str);
    to_upper_ascii(result);
    return result;
}

// ============================================================================
// Comparison
// ============================================================================

bool equals_ignore_case_ascii(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];

        // Convert to lowercase
        if (ca >= 'A' && ca <= 'Z')
            ca = ca - 'A' + 'a';
        if (cb >= 'A' && cb <= 'Z')
            cb = cb - 'a' + 'a';

        if (ca != cb)
            return false;
    }

    return true;
}

bool starts_with_ignore_case_ascii(std::string_view str, std::string_view prefix) noexcept {
    if (str.size() < prefix.size())
        return false;
    return equals_ignore_case_ascii(str.substr(0, prefix.size()), prefix);
}

// ============================================================================
// Splitting
// ============================================================================

std::vector<std::string_view> split(std::string_view str, char delimiter) {
    std::vector<std::string_view> result;

    size_t start = 0;
    size_t pos = 0;

    while ((pos = str.find(delimiter, start)) != std::string_view::npos) {
        result.push_back(str.substr(start, pos - start));
        start = pos + 1;
    }

    result.push_back(str.substr(start));
    return result;
}

std::vector<std::string_view> split(std::string_view str, std::string_view delimiter) {
    std::vector<std::string_view> result;

    if (delimiter.empty()) {
        result.push_back(str);
        return result;
    }

    size_t start = 0;
    size_t pos = 0;

    while ((pos = str.find(delimiter, start)) != std::string_view::npos) {
        result.push_back(str.substr(start, pos - start));
        start = pos + delimiter.size();
    }

    result.push_back(str.substr(start));
    return result;
}

std::vector<std::string_view> split_n(std::string_view str, char delimiter, size_t max_parts) {
    std::vector<std::string_view> result;

    if (max_parts == 0)
        return result;

    size_t start = 0;
    size_t pos = 0;

    while (result.size() < max_parts - 1 && (pos = str.find(delimiter, start)) != std::string_view::npos) {
        result.push_back(str.substr(start, pos - start));
        start = pos + 1;
    }

    result.push_back(str.substr(start));
    return result;
}

std::vector<std::string_view> split_any(std::string_view str, std::string_view delimiters) {
    std::vector<std::string_view> result;

    size_t start = 0;
    size_t pos = 0;

    while ((pos = str.find_first_of(delimiters, start)) != std::string_view::npos) {
        if (pos > start) {
            result.push_back(str.substr(start, pos - start));
        }
        start = pos + 1;
    }

    if (start < str.size()) {
        result.push_back(str.substr(start));
    }

    return result;
}

// ============================================================================
// Search and replace
// ============================================================================

std::string replace_all(std::string_view str, std::string_view from, std::string_view to) {
    if (from.empty())
        return std::string(str);

    std::string result;
    result.reserve(str.size());

    size_t start = 0;
    size_t pos = 0;

    while ((pos = str.find(from, start)) != std::string_view::npos) {
        result.append(str.substr(start, pos - start));
        result.append(to);
        start = pos + from.size();
    }

    result.append(str.substr(start));
    return result;
}

std::string replace_first(std::string_view str, std::string_view from, std::string_view to) {
    size_t pos = str.find(from);
    if (pos == std::string_view::npos) {
        return std::string(str);
    }

    std::string result;
    result.reserve(str.size() - from.size() + to.size());
    result.append(str.substr(0, pos));
    result.append(to);
    result.append(str.substr(pos + from.size()));
    return result;
}

size_t count_occurrences(std::string_view str, std::string_view substr) noexcept {
    if (substr.empty())
        return 0;

    size_t count = 0;
    size_t pos = 0;

    while ((pos = str.find(substr, pos)) != std::string_view::npos) {
        ++count;
        pos += substr.size();
    }

    return count;
}

std::vector<size_t> find_all(std::string_view str, std::string_view substr) {
    std::vector<size_t> positions;

    if (substr.empty())
        return positions;

    size_t pos = 0;
    while ((pos = str.find(substr, pos)) != std::string_view::npos) {
        positions.push_back(pos);
        pos += substr.size();
    }

    return positions;
}

// ============================================================================
// HTML entities
// ============================================================================

std::string decode_html_entities(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    size_t i = 0;
    while (i < str.size()) {
        if (str[i] == '&') {
            size_t end = str.find(';', i + 1);
            if (end != std::string_view::npos && end - i < 12) {
                std::string_view entity = str.substr(i + 1, end - i - 1);

                // Named entities
                if (entity == "amp") {
                    result += '&';
                    i = end + 1;
                    continue;
                }
                if (entity == "lt") {
                    result += '<';
                    i = end + 1;
                    continue;
                }
                if (entity == "gt") {
                    result += '>';
                    i = end + 1;
                    continue;
                }
                if (entity == "quot") {
                    result += '"';
                    i = end + 1;
                    continue;
                }
                if (entity == "apos") {
                    result += '\'';
                    i = end + 1;
                    continue;
                }
                if (entity == "nbsp") {
                    result += ' ';
                    i = end + 1;
                    continue;
                }

                // Numeric entities
                if (entity.size() > 1 && entity[0] == '#') {
                    int codepoint = 0;
                    if (entity[1] == 'x' || entity[1] == 'X') {
                        // Hex
                        for (size_t j = 2; j < entity.size(); ++j) {
                            char c = entity[j];
                            if (c >= '0' && c <= '9')
                                codepoint = codepoint * 16 + (c - '0');
                            else if (c >= 'a' && c <= 'f')
                                codepoint = codepoint * 16 + (c - 'a' + 10);
                            else if (c >= 'A' && c <= 'F')
                                codepoint = codepoint * 16 + (c - 'A' + 10);
                        }
                    } else {
                        // Decimal
                        for (size_t j = 1; j < entity.size(); ++j) {
                            char c = entity[j];
                            if (c >= '0' && c <= '9')
                                codepoint = codepoint * 10 + (c - '0');
                        }
                    }

                    if (codepoint > 0 && codepoint < 0x110000) {
                        // Encode as UTF-8
                        if (codepoint < 0x80) {
                            result += static_cast<char>(codepoint);
                        } else if (codepoint < 0x800) {
                            result += static_cast<char>(0xC0 | (codepoint >> 6));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } else if (codepoint < 0x10000) {
                            result += static_cast<char>(0xE0 | (codepoint >> 12));
                            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } else {
                            result += static_cast<char>(0xF0 | (codepoint >> 18));
                            result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        }
                        i = end + 1;
                        continue;
                    }
                }
            }
        }
        result += str[i++];
    }

    return result;
}

std::string encode_html_entities(std::string_view str) {
    std::string result;
    result.reserve(str.size() + str.size() / 10); // Assume ~10% expansion

    for (char c: str) {
        switch (c) {
            case '&':
                result += "&amp;";
                break;
            case '<':
                result += "&lt;";
                break;
            case '>':
                result += "&gt;";
                break;
            case '"':
                result += "&quot;";
                break;
            case '\'':
                result += "&#39;";
                break;
            default:
                result += c;
                break;
        }
    }

    return result;
}

std::string strip_tags(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    bool in_tag = false;
    for (char c: str) {
        if (c == '<') {
            in_tag = true;
        } else if (c == '>') {
            in_tag = false;
        } else if (!in_tag) {
            result += c;
        }
    }

    return result;
}

// ============================================================================
// Line handling
// ============================================================================

std::vector<std::string_view> split_lines(std::string_view str) {
    std::vector<std::string_view> result;

    size_t start = 0;
    size_t i = 0;

    while (i < str.size()) {
        if (str[i] == '\r') {
            result.push_back(str.substr(start, i - start));
            if (i + 1 < str.size() && str[i + 1] == '\n') {
                ++i; // Skip \n in \r\n
            }
            start = i + 1;
        } else if (str[i] == '\n') {
            result.push_back(str.substr(start, i - start));
            start = i + 1;
        }
        ++i;
    }

    if (start <= str.size()) {
        result.push_back(str.substr(start));
    }

    return result;
}

size_t count_lines(std::string_view str) noexcept {
    if (str.empty())
        return 0;

    size_t count = 1;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\n') {
            ++count;
        } else if (str[i] == '\r') {
            ++count;
            if (i + 1 < str.size() && str[i + 1] == '\n') {
                ++i; // Skip \n in \r\n
            }
        }
    }

    return count;
}

std::optional<std::string_view> get_line(std::string_view str, size_t line_number) {
    auto lines = split_lines(str);
    if (line_number < lines.size()) {
        return lines[line_number];
    }
    return std::nullopt;
}

} // namespace wikilib::text

// ============================================================================
// ParseError implementation
// ============================================================================

namespace wikilib {

std::string ParseError::format() const {
    std::string result;

    // Severity prefix
    switch (severity) {
        case ErrorSeverity::Warning:
            result = "Warning: ";
            break;
        case ErrorSeverity::Error:
            result = "Error: ";
            break;
        case ErrorSeverity::Fatal:
            result = "Fatal: ";
            break;
    }

    result += message;

    // Add location if available
    if (location.begin.line > 0) {
        result += " at line ";
        result += std::to_string(location.begin.line);
        result += ", column ";
        result += std::to_string(location.begin.column);
    }

    // Add context if available
    if (!context.empty()) {
        result += "\n  Context: ";
        result += context;
    }

    return result;
}

} // namespace wikilib
