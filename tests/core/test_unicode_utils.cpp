#include <gtest/gtest.h>
#include "wikilib/core/unicode_utils.h"

using namespace wikilib::unicode;

// ============================================================================
// UTF-8 validation tests
// ============================================================================

TEST(UnicodeUtilsTest, IsValidUtf8_Valid) {
    EXPECT_TRUE(is_valid_utf8(""));
    EXPECT_TRUE(is_valid_utf8("Hello"));
    EXPECT_TRUE(is_valid_utf8("Привет")); // Russian
    EXPECT_TRUE(is_valid_utf8("こんにちは")); // Japanese
    EXPECT_TRUE(is_valid_utf8("你好")); // Chinese
    EXPECT_TRUE(is_valid_utf8("مرحبا")); // Arabic
    EXPECT_TRUE(is_valid_utf8("🎉🚀")); // Emoji (4-byte sequences)
}

TEST(UnicodeUtilsTest, IsValidUtf8_Invalid) {
    // Invalid continuation byte
    EXPECT_FALSE(is_valid_utf8("\xFF"));
    EXPECT_FALSE(is_valid_utf8("\x80"));

    // Truncated sequences
    EXPECT_FALSE(is_valid_utf8("\xC2")); // Missing continuation byte
    EXPECT_FALSE(is_valid_utf8("\xE0\x80")); // Missing second continuation
    EXPECT_FALSE(is_valid_utf8("\xF0\x80\x80")); // Missing third continuation

    // Overlong encodings
    EXPECT_FALSE(is_valid_utf8("\xC0\x80")); // Overlong encoding of NULL
    EXPECT_FALSE(is_valid_utf8("\xE0\x80\x80")); // Overlong 3-byte

    // UTF-16 surrogates (invalid in UTF-8)
    EXPECT_FALSE(is_valid_utf8("\xED\xA0\x80")); // U+D800
}

TEST(UnicodeUtilsTest, Utf8CharLength) {
    EXPECT_EQ(utf8_char_length('a'), 1u); // ASCII
    EXPECT_EQ(utf8_char_length('\xC2'), 2u); // 2-byte start
    EXPECT_EQ(utf8_char_length('\xE0'), 3u); // 3-byte start
    EXPECT_EQ(utf8_char_length('\xF0'), 4u); // 4-byte start
    EXPECT_EQ(utf8_char_length('\x80'), 0u); // Invalid (continuation byte)
    EXPECT_EQ(utf8_char_length('\xFF'), 0u); // Invalid
}

TEST(UnicodeUtilsTest, CountCodepoints) {
    EXPECT_EQ(count_codepoints(""), 0u);
    EXPECT_EQ(count_codepoints("hello"), 5u);
    EXPECT_EQ(count_codepoints("Привет"), 6u); // 6 Cyrillic chars
    EXPECT_EQ(count_codepoints("你好"), 2u); // 2 Chinese chars
    EXPECT_EQ(count_codepoints("🎉🚀"), 2u); // 2 emoji
    EXPECT_EQ(count_codepoints("a🎉b"), 3u); // Mixed ASCII and emoji
}

TEST(UnicodeUtilsTest, DecodeUtf8) {
    std::string str = "a你🎉";
    size_t pos = 0;

    auto cp1 = decode_utf8(str, pos);
    ASSERT_TRUE(cp1.has_value());
    EXPECT_EQ(*cp1, U'a');
    EXPECT_EQ(pos, 1u);

    auto cp2 = decode_utf8(str, pos);
    ASSERT_TRUE(cp2.has_value());
    EXPECT_EQ(*cp2, U'你');
    EXPECT_EQ(pos, 4u); // 1 + 3 bytes

    auto cp3 = decode_utf8(str, pos);
    ASSERT_TRUE(cp3.has_value());
    EXPECT_EQ(*cp3, U'🎉');

    auto cp4 = decode_utf8(str, pos);
    EXPECT_FALSE(cp4.has_value()); // End of string
}

TEST(UnicodeUtilsTest, EncodeUtf8) {
    EXPECT_EQ(encode_utf8(U'a'), "a");
    EXPECT_EQ(encode_utf8(U'你'), "你");
    EXPECT_EQ(encode_utf8(U'🎉'), "🎉");
    EXPECT_EQ(encode_utf8(U'\u00A9'), "©"); // Copyright symbol
    EXPECT_EQ(encode_utf8(U'\u0041'), "A"); // ASCII 'A'
}

// ============================================================================
// Case conversion tests
// ============================================================================

TEST(UnicodeUtilsTest, ToLower) {
    EXPECT_EQ(to_lower("HELLO"), "hello");
    EXPECT_EQ(to_lower("Hello World"), "hello world");
    EXPECT_EQ(to_lower("ПРИВЕТ"), "привет"); // Russian
    EXPECT_EQ(to_lower("ÑOÑO"), "ñoño"); // Spanish
    EXPECT_EQ(to_lower("İSTANBUL"), "i̇stanbul"); // Turkish (dotted I)
    EXPECT_EQ(to_lower("123"), "123"); // Numbers unchanged
}

TEST(UnicodeUtilsTest, ToUpper) {
    EXPECT_EQ(to_upper("hello"), "HELLO");
    EXPECT_EQ(to_upper("Hello World"), "HELLO WORLD");
    EXPECT_EQ(to_upper("привет"), "ПРИВЕТ"); // Russian
    EXPECT_EQ(to_upper("ñoño"), "ÑOÑO"); // Spanish
    EXPECT_EQ(to_upper("straße"), "STRASSE"); // German ß -> SS
}

TEST(UnicodeUtilsTest, ToTitleCase) {
    EXPECT_EQ(to_title_case("hello world"), "Hello World");
    EXPECT_EQ(to_title_case("HELLO WORLD"), "Hello World");
    EXPECT_EQ(to_title_case("hello"), "Hello");
}

TEST(UnicodeUtilsTest, CapitalizeFirst) {
    EXPECT_EQ(capitalize_first("hello"), "Hello");
    EXPECT_EQ(capitalize_first("HELLO"), "HELLO"); // Only first char
    EXPECT_EQ(capitalize_first("привет"), "Привет"); // Russian
    EXPECT_EQ(capitalize_first(""), "");
    EXPECT_EQ(capitalize_first("a"), "A");
    EXPECT_EQ(capitalize_first("élève"), "Élève"); // French
}

// ============================================================================
// Normalization tests
// ============================================================================

TEST(UnicodeUtilsTest, NormalizeNFC) {
    // é can be represented as single char (U+00E9) or e + combining acute (U+0065 U+0301)
    std::string composed = "é"; // U+00E9
    std::string decomposed = "e\u0301"; // e + combining acute

    auto normalized1 = normalize(composed, NormalizationForm::NFC);
    auto normalized2 = normalize(decomposed, NormalizationForm::NFC);

    EXPECT_EQ(normalized1, normalized2); // Should be equal after NFC
    EXPECT_EQ(normalized1, "é");
}

TEST(UnicodeUtilsTest, NormalizeNFD) {
    std::string composed = "é"; // U+00E9

    auto normalized = normalize(composed, NormalizationForm::NFD);

    // After NFD, should be decomposed
    EXPECT_GT(normalized.size(), composed.size());
}

TEST(UnicodeUtilsTest, NormalizeNFKC) {
    // NFKC performs compatibility decomposition + composition
    std::string input = "ﬁ"; // U+FB01 (ligature fi)
    auto normalized = normalize(input, NormalizationForm::NFKC);

    // Should decompose ligature to separate chars
    EXPECT_EQ(normalized, "fi");
}

// ============================================================================
// Character classification tests
// ============================================================================

TEST(UnicodeUtilsTest, IsWhitespace) {
    EXPECT_TRUE(is_whitespace(U' '));
    EXPECT_TRUE(is_whitespace(U'\t'));
    EXPECT_TRUE(is_whitespace(U'\n'));
    EXPECT_TRUE(is_whitespace(U'\u00A0')); // Non-breaking space
    EXPECT_TRUE(is_whitespace(U'\u2003')); // Em space
    EXPECT_FALSE(is_whitespace(U'a'));
    EXPECT_FALSE(is_whitespace(U'0'));
}

TEST(UnicodeUtilsTest, IsLetter) {
    EXPECT_TRUE(is_letter(U'a'));
    EXPECT_TRUE(is_letter(U'Z'));
    EXPECT_TRUE(is_letter(U'á'));
    EXPECT_TRUE(is_letter(U'Ω')); // Greek
    EXPECT_TRUE(is_letter(U'你')); // Chinese
    EXPECT_FALSE(is_letter(U'0'));
    EXPECT_FALSE(is_letter(U' '));
    EXPECT_FALSE(is_letter(U'!'));
}

TEST(UnicodeUtilsTest, IsDigit) {
    EXPECT_TRUE(is_digit(U'0'));
    EXPECT_TRUE(is_digit(U'9'));
    EXPECT_TRUE(is_digit(U'٥')); // Arabic-Indic digit 5
    EXPECT_FALSE(is_digit(U'a'));
    EXPECT_FALSE(is_digit(U' '));
}

TEST(UnicodeUtilsTest, IsAlphanumeric) {
    EXPECT_TRUE(is_alphanumeric(U'a'));
    EXPECT_TRUE(is_alphanumeric(U'Z'));
    EXPECT_TRUE(is_alphanumeric(U'0'));
    EXPECT_TRUE(is_alphanumeric(U'9'));
    EXPECT_TRUE(is_alphanumeric(U'á'));
    EXPECT_FALSE(is_alphanumeric(U' '));
    EXPECT_FALSE(is_alphanumeric(U'!'));
}

TEST(UnicodeUtilsTest, IsPunctuation) {
    EXPECT_TRUE(is_punctuation(U'.'));
    EXPECT_TRUE(is_punctuation(U','));
    EXPECT_TRUE(is_punctuation(U'!'));
    EXPECT_TRUE(is_punctuation(U'?'));
    EXPECT_TRUE(is_punctuation(U';'));
    EXPECT_FALSE(is_punctuation(U'a'));
    EXPECT_FALSE(is_punctuation(U'0'));
    EXPECT_FALSE(is_punctuation(U' '));
}

// ============================================================================
// MediaWiki-specific tests
// ============================================================================

TEST(UnicodeUtilsTest, NormalizeTitle) {
    EXPECT_EQ(normalize_title("  hello  "), "Hello");
    EXPECT_EQ(normalize_title("hello_world"), "Hello world");
    EXPECT_EQ(normalize_title("hello   world"), "Hello world");
    EXPECT_EQ(normalize_title("test#section"), "Test"); // Remove fragment
    EXPECT_EQ(normalize_title("élève"), "Élève"); // Unicode capitalize
    EXPECT_EQ(normalize_title(""), "");
}

TEST(UnicodeUtilsTest, NormalizeForComparison) {
    // First character lowercased for comparison
    EXPECT_EQ(normalize_for_comparison("Hello"), "hello");
    EXPECT_EQ(normalize_for_comparison("HELLO"), "hELLO"); // Only first char
    EXPECT_EQ(normalize_for_comparison("Привет"), "привет"); // Russian
    EXPECT_EQ(normalize_for_comparison("Élève"), "élève"); // French
}

TEST(UnicodeUtilsTest, TitleToUrl) {
    EXPECT_EQ(title_to_url("Hello World"), "Hello_World");
    EXPECT_EQ(title_to_url("Test/Page"), "Test%2FPage");
    EXPECT_EQ(title_to_url("C++"), "C%2B%2B");
    EXPECT_EQ(title_to_url("café"), "caf%C3%A9");
}

TEST(UnicodeUtilsTest, UrlToTitle) {
    EXPECT_EQ(url_to_title("Hello_World"), "Hello World");
    EXPECT_EQ(url_to_title("Test%2FPage"), "Test/Page");
    EXPECT_EQ(url_to_title("C%2B%2B"), "C++");
    EXPECT_EQ(url_to_title("caf%C3%A9"), "café");
}

TEST(UnicodeUtilsTest, UrlRoundTrip) {
    std::string original = "Test Page/With:Special#Chars";
    std::string encoded = title_to_url(original);
    std::string decoded = url_to_title(encoded);

    // After round-trip, special chars are preserved but spaces become underscores
    std::string expected = original;
    std::replace(expected.begin(), expected.end(), ' ', '_');
    std::replace(decoded.begin(), decoded.end(), ' ', '_');

    EXPECT_EQ(decoded, expected);
}

// ============================================================================
// UTF-8 iterator tests
// ============================================================================

TEST(UnicodeUtilsTest, Utf8Iterator_Basic) {
    std::string str = "abc";
    Utf8Iterator it(str, 0);
    Utf8Iterator end(str, str.size());

    ASSERT_NE(it, end);
    EXPECT_EQ(*it, U'a');

    ++it;
    ASSERT_NE(it, end);
    EXPECT_EQ(*it, U'b');

    ++it;
    ASSERT_NE(it, end);
    EXPECT_EQ(*it, U'c');

    ++it;
    EXPECT_EQ(it, end);
}

TEST(UnicodeUtilsTest, Utf8Iterator_Unicode) {
    std::string str = "a你🎉";
    Utf8Iterator it(str, 0);
    Utf8Iterator end(str, str.size());

    ASSERT_NE(it, end);
    EXPECT_EQ(*it, U'a');

    ++it;
    ASSERT_NE(it, end);
    EXPECT_EQ(*it, U'你');

    ++it;
    ASSERT_NE(it, end);
    EXPECT_EQ(*it, U'🎉');

    ++it;
    EXPECT_EQ(it, end);
}

TEST(UnicodeUtilsTest, Utf8View_RangeFor) {
    std::string str = "a你b";
    auto view = utf8_codepoints(str);

    std::vector<char32_t> codepoints;
    for (char32_t cp : view) {
        codepoints.push_back(cp);
    }

    ASSERT_EQ(codepoints.size(), 3u);
    EXPECT_EQ(codepoints[0], U'a');
    EXPECT_EQ(codepoints[1], U'你');
    EXPECT_EQ(codepoints[2], U'b');
}

TEST(UnicodeUtilsTest, Utf8View_Empty) {
    std::string str = "";
    auto view = utf8_codepoints(str);

    int count = 0;
    for (char32_t cp : view) {
        (void)cp;
        ++count;
    }

    EXPECT_EQ(count, 0);
}

TEST(UnicodeUtilsTest, Utf8Iterator_PostIncrement) {
    std::string str = "abc";
    Utf8Iterator it(str, 0);

    auto old_it = it++;
    EXPECT_EQ(*old_it, U'a');
    EXPECT_EQ(*it, U'b');
}

TEST(UnicodeUtilsTest, Utf8Iterator_BytePosition) {
    std::string str = "a你b"; // 1 + 3 + 1 bytes
    Utf8Iterator it(str, 0);

    // Iterator points AT the current character, not past it
    EXPECT_EQ(*it, U'a');
    EXPECT_EQ(it.byte_position(), 0u); // At 'a'

    ++it;
    EXPECT_EQ(*it, U'你');
    EXPECT_EQ(it.byte_position(), 1u); // At '你' (after 1-byte 'a')

    ++it;
    EXPECT_EQ(*it, U'b');
    EXPECT_EQ(it.byte_position(), 4u); // At 'b' (after 'a' + '你')

    ++it;
    // End iterator
    EXPECT_EQ(it.byte_position(), 5u); // Past end
}
