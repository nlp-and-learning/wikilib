/**
 * @file unicode_utils.cpp
 * @brief Implementation of Unicode utilities using ICU
 */

#include "wikilib/core/unicode_utils.h"
#include "wikilib/core/text_utils.hpp"

#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/normalizer2.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>
#include <unicode/ucasemap.h>

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace wikilib::unicode {

// ============================================================================
// UTF-8 validation and decoding
// ============================================================================

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

        // Additional validation: check for overlong encodings and invalid ranges
        if (len == 2) {
            if ((c & 0x1E) == 0)
                return false; // Overlong
        } else if (len == 3) {
            if (c == 0xE0 && (str[i + 1] & 0xE0) == 0x80)
                return false; // Overlong
            if (c == 0xED && (str[i + 1] & 0xE0) == 0xA0)
                return false; // UTF-16 surrogates
        } else if (len == 4) {
            if (c == 0xF0 && (str[i + 1] & 0xF0) == 0x80)
                return false; // Overlong
            if (c >= 0xF5)
                return false; // Out of Unicode range
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

std::optional<char32_t> decode_utf8(std::string_view str, size_t &pos) noexcept {
    if (pos >= str.size())
        return std::nullopt;

    unsigned char first = str[pos];
    size_t len = utf8_char_length(first);

    if (len == 0 || pos + len > str.size())
        return std::nullopt;

    char32_t cp = 0;

    switch (len) {
        case 1:
            cp = first;
            break;
        case 2:
            cp = ((first & 0x1F) << 6) | (str[pos + 1] & 0x3F);
            break;
        case 3:
            cp = ((first & 0x0F) << 12) | ((str[pos + 1] & 0x3F) << 6) | (str[pos + 2] & 0x3F);
            break;
        case 4:
            cp = ((first & 0x07) << 18) | ((str[pos + 1] & 0x3F) << 12) | ((str[pos + 2] & 0x3F) << 6) |
                 (str[pos + 3] & 0x3F);
            break;
    }

    pos += len;
    return cp;
}

std::string encode_utf8(char32_t codepoint) {
    std::string result;

    if (codepoint <= 0x7F) {
        result += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
        result += static_cast<char>(0xC0 | (codepoint >> 6));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        result += static_cast<char>(0xE0 | (codepoint >> 12));
        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
        result += static_cast<char>(0xF0 | (codepoint >> 18));
        result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    }

    return result;
}

// ============================================================================
// Case conversion (using ICU)
// ============================================================================

std::string to_lower(std::string_view str) {
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(icu::StringPiece(str.data(), static_cast<int32_t>(str.size())));
    ustr.toLower();

    std::string result;
    ustr.toUTF8String(result);
    return result;
}

std::string to_upper(std::string_view str) {
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(icu::StringPiece(str.data(), static_cast<int32_t>(str.size())));
    ustr.toUpper();

    std::string result;
    ustr.toUTF8String(result);
    return result;
}

std::string to_title_case(std::string_view str) {
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(icu::StringPiece(str.data(), static_cast<int32_t>(str.size())));
    ustr.toTitle(nullptr);

    std::string result;
    ustr.toUTF8String(result);
    return result;
}

std::string capitalize_first(std::string_view str) {
    if (str.empty())
        return {};

    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(icu::StringPiece(str.data(), static_cast<int32_t>(str.size())));

    if (ustr.length() > 0) {
        UChar32 first = ustr.char32At(0);
        UChar32 upper = u_toupper(first);

        if (first != upper) {
            // Replace the first character with its uppercase version
            ustr.replace(0, 1, upper);
        }
    }

    std::string result;
    ustr.toUTF8String(result);
    return result;
}

// ============================================================================
// Normalization (using ICU)
// ============================================================================

std::string normalize(std::string_view str, NormalizationForm form) {
    UErrorCode status = U_ZERO_ERROR;

    const icu::Normalizer2 *normalizer = nullptr;

    switch (form) {
        case NormalizationForm::NFC:
            normalizer = icu::Normalizer2::getNFCInstance(status);
            break;
        case NormalizationForm::NFD:
            normalizer = icu::Normalizer2::getNFDInstance(status);
            break;
        case NormalizationForm::NFKC:
            normalizer = icu::Normalizer2::getNFKCInstance(status);
            break;
        case NormalizationForm::NFKD:
            normalizer = icu::Normalizer2::getNFKDInstance(status);
            break;
    }

    if (U_FAILURE(status) || !normalizer) {
        return std::string(str); // Return unchanged on error
    }

    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(icu::StringPiece(str.data(), static_cast<int32_t>(str.size())));
    icu::UnicodeString normalized = normalizer->normalize(ustr, status);

    if (U_FAILURE(status)) {
        return std::string(str);
    }

    std::string result;
    normalized.toUTF8String(result);
    return result;
}

// ============================================================================
// Character classification (using ICU)
// ============================================================================

bool is_whitespace(char32_t cp) noexcept {
    return u_isUWhiteSpace(static_cast<UChar32>(cp));
}

bool is_letter(char32_t cp) noexcept {
    return u_isalpha(static_cast<UChar32>(cp));
}

bool is_digit(char32_t cp) noexcept {
    return u_isdigit(static_cast<UChar32>(cp));
}

bool is_alphanumeric(char32_t cp) noexcept {
    return u_isalnum(static_cast<UChar32>(cp));
}

bool is_punctuation(char32_t cp) noexcept {
    return u_ispunct(static_cast<UChar32>(cp));
}

// ============================================================================
// MediaWiki-specific normalization
// ============================================================================

std::string normalize_title(std::string_view title) {
    std::string result(text::trim(title));

    // Replace underscores with spaces
    for (char &c: result) {
        if (c == '_')
            c = ' ';
    }

    // Collapse whitespace
    result = text::collapse_whitespace(result);

    // Remove fragment (#section)
    if (auto pos = result.find('#'); pos != std::string::npos) {
        result = result.substr(0, pos);
        result = text::trim(result);
    }

    // Capitalize first letter using ICU
    if (!result.empty()) {
        result = capitalize_first(result);
    }

    return result;
}

std::string normalize_for_comparison(std::string_view str) {
    // MediaWiki titles are case-sensitive except for first character
    // So we normalize the first character to lowercase for comparison
    if (str.empty())
        return {};

    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(icu::StringPiece(str.data(), static_cast<int32_t>(str.size())));

    if (ustr.length() > 0) {
        UChar32 first = ustr.char32At(0);
        UChar32 lower = u_tolower(first);
        if (first != lower) {
            // Replace the first character with its lowercase version
            ustr.replace(0, 1, lower);
        }
    }

    std::string result;
    ustr.toUTF8String(result);
    return result;
}

std::string title_to_url(std::string_view title) {
    std::ostringstream result;

    for (char ch : title) {
        auto c = static_cast<unsigned char>(ch);

        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            // Safe characters
            result << ch;
        } else if (c == ' ') {
            // Space becomes underscore
            result << '_';
        } else {
            // Percent-encode everything else
            result << '%' << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
    }

    return result.str();
}

std::string url_to_title(std::string_view url) {
    std::string result;
    result.reserve(url.size());

    for (size_t i = 0; i < url.size(); ++i) {
        if (url[i] == '%' && i + 2 < url.size()) {
            // Decode percent-encoded character
            char hex[3] = {url[i + 1], url[i + 2], '\0'};
            char *end;
            long val = std::strtol(hex, &end, 16);
            if (end == hex + 2) {
                result += static_cast<char>(val);
                i += 2;
            } else {
                result += url[i];
            }
        } else if (url[i] == '_') {
            // Underscore becomes space
            result += ' ';
        } else {
            result += url[i];
        }
    }

    return result;
}

// ============================================================================
// UTF-8 Iterator implementation
// ============================================================================

Utf8Iterator::Utf8Iterator(std::string_view str, size_t pos) : str_(str), pos_(pos) {
    if (pos_ < str_.size()) {
        advance();
    }
}

char32_t Utf8Iterator::operator*() const {
    return current_;
}

Utf8Iterator &Utf8Iterator::operator++() {
    advance();
    return *this;
}

Utf8Iterator Utf8Iterator::operator++(int) {
    Utf8Iterator tmp = *this;
    advance();
    return tmp;
}

bool Utf8Iterator::operator==(const Utf8Iterator &other) const {
    return str_.data() == other.str_.data() && pos_ == other.pos_;
}

bool Utf8Iterator::operator!=(const Utf8Iterator &other) const {
    return !(*this == other);
}

void Utf8Iterator::advance() {
    if (pos_ >= str_.size()) {
        current_ = 0;
        return;
    }

    auto cp = decode_utf8(str_, pos_);
    current_ = cp.value_or(0xFFFD); // Replacement character on error
}

} // namespace wikilib::unicode
