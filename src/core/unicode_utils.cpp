/**
 * @file unicode_utils.cpp
 * @brief Implementation of Unicode utilities
 */

#include "wikilib/core/unicode_utils.h"
#include "wikilib/core/text_utils.hpp"

namespace wikilib::unicode {

// TODO: Full implementation
// For now, basic implementations without ICU

bool is_valid_utf8(std::string_view str) noexcept {
    size_t i = 0;
    while (i < str.size()) {
        unsigned char c = str[i];
        size_t len = utf8_char_length(c);
        if (len == 0 || i + len > str.size())
            return false;

        // Validate continuation bytes
        for (size_t j = 1; j < len; ++j) {
            if ((str[i + j] & 0xC0) != 0x80)
                return false;
        }
        i += len;
    }
    return true;
}

size_t utf8_char_length(char first_byte) noexcept {
    unsigned char c = static_cast<unsigned char>(first_byte);
    if ((c & 0x80) == 0)
        return 1;
    if ((c & 0xE0) == 0xC0)
        return 2;
    if ((c & 0xF0) == 0xE0)
        return 3;
    if ((c & 0xF8) == 0xF0)
        return 4;
    return 0; // Invalid
}

size_t count_codepoints(std::string_view str) noexcept {
    size_t count = 0;
    for (size_t i = 0; i < str.size();) {
        size_t len = utf8_char_length(str[i]);
        if (len == 0)
            len = 1; // Skip invalid byte
        i += len;
        ++count;
    }
    return count;
}

std::string normalize_title(std::string_view title) {
    std::string result(text::trim(title));

    // Replace underscores with spaces
    for (char &c: result) {
        if (c == '_')
            c = ' ';
    }

    // Collapse whitespace
    result = text::collapse_whitespace(result);

    // Capitalize first letter (ASCII only for now)
    if (!result.empty() && result[0] >= 'a' && result[0] <= 'z') {
        result[0] = result[0] - 'a' + 'A';
    }

    return result;
}

std::string capitalize_first(std::string_view str) {
    if (str.empty())
        return {};
    std::string result(str);

    // ASCII only for now
    if (result[0] >= 'a' && result[0] <= 'z') {
        result[0] = result[0] - 'a' + 'A';
    }

    return result;
}

} // namespace wikilib::unicode
