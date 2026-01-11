#include <gtest/gtest.h>
#include "wikilib/core/string_pool.h"

using namespace wikilib;

// ============================================================================
// InternedString tests
// ============================================================================

TEST(InternedStringTest, DefaultConstruction) {
    InternedString str;
    EXPECT_TRUE(str.empty());
    EXPECT_EQ(str.size(), 0u);
    EXPECT_EQ(str.view(), "");
    EXPECT_FALSE(str);
}

TEST(InternedStringTest, Comparison) {
    StringPool pool;
    auto str1 = pool.intern("hello");
    auto str2 = pool.intern("hello");
    auto str3 = pool.intern("world");

    // Same string should have same pointer
    EXPECT_EQ(str1, str2);
    EXPECT_NE(str1, str3);

    // Comparison with string_view
    EXPECT_EQ(str1, "hello");
    EXPECT_NE(str1, "world");
}

TEST(InternedStringTest, ViewAccess) {
    StringPool pool;
    auto str = pool.intern("hello world");

    EXPECT_EQ(str.view(), "hello world");
    EXPECT_EQ(str.size(), 11u);
    EXPECT_FALSE(str.empty());
    EXPECT_TRUE(str);

    // c_str should be null-terminated
    std::string_view sv(str.c_str());
    EXPECT_EQ(sv, "hello world");
}

// ============================================================================
// StringPool tests
// ============================================================================

TEST(StringPoolTest, BasicIntern) {
    StringPool pool;

    auto str1 = pool.intern("test");
    EXPECT_EQ(str1.view(), "test");
    EXPECT_EQ(pool.size(), 1u);

    auto str2 = pool.intern("test");
    EXPECT_EQ(str2.view(), "test");
    EXPECT_EQ(pool.size(), 1u); // Should reuse existing

    // Same pointer for same string
    EXPECT_EQ(str1, str2);
}

TEST(StringPoolTest, MultipleDifferentStrings) {
    StringPool pool;

    auto str1 = pool.intern("one");
    auto str2 = pool.intern("two");
    auto str3 = pool.intern("three");

    EXPECT_EQ(pool.size(), 3u);
    EXPECT_NE(str1, str2);
    EXPECT_NE(str2, str3);
    EXPECT_NE(str1, str3);
}

TEST(StringPoolTest, EmptyString) {
    StringPool pool;

    auto str1 = pool.intern("");
    auto str2 = pool.intern("");

    EXPECT_TRUE(str1.empty());
    EXPECT_EQ(str1, str2);
    EXPECT_EQ(pool.size(), 1u);
}

TEST(StringPoolTest, Contains) {
    StringPool pool;

    pool.intern("hello");
    pool.intern("world");

    EXPECT_TRUE(pool.contains("hello"));
    EXPECT_TRUE(pool.contains("world"));
    EXPECT_FALSE(pool.contains("foo"));
}

TEST(StringPoolTest, Find) {
    StringPool pool;

    pool.intern("hello");

    auto found = pool.find("hello");
    EXPECT_TRUE(found);
    EXPECT_EQ(found.view(), "hello");

    auto not_found = pool.find("world");
    EXPECT_FALSE(not_found);
}

TEST(StringPoolTest, Clear) {
    StringPool pool;

    auto str1 = pool.intern("one");
    auto str2 = pool.intern("two");

    EXPECT_EQ(pool.size(), 2u);

    pool.clear();

    EXPECT_EQ(pool.size(), 0u);
    EXPECT_FALSE(pool.contains("one"));
    EXPECT_FALSE(pool.contains("two"));

    // Note: str1 and str2 are now dangling - don't use them!
}

TEST(StringPoolTest, MemoryUsage) {
    StringPool pool;

    pool.intern("a");
    pool.intern("ab");
    pool.intern("abc");

    size_t usage = pool.memory_usage();
    // Should be at least the sum of string lengths
    EXPECT_GE(usage, 6u); // 1 + 2 + 3
}

TEST(StringPoolTest, Reserve) {
    StringPool pool;

    // Should not crash
    pool.reserve(100);

    for (int i = 0; i < 100; ++i) {
        pool.intern("string" + std::to_string(i));
    }

    EXPECT_EQ(pool.size(), 100u);
}

TEST(StringPoolTest, UnicodeStrings) {
    StringPool pool;

    auto str1 = pool.intern("Привет");
    auto str2 = pool.intern("こんにちは");
    auto str3 = pool.intern("你好");
    auto str4 = pool.intern("مرحبا");

    EXPECT_EQ(str1.view(), "Привет");
    EXPECT_EQ(str2.view(), "こんにちは");
    EXPECT_EQ(str3.view(), "你好");
    EXPECT_EQ(str4.view(), "مرحبا");

    EXPECT_EQ(pool.size(), 4u);
}

TEST(StringPoolTest, LongStrings) {
    StringPool pool;

    std::string long_str(10000, 'x');
    auto str = pool.intern(long_str);

    EXPECT_EQ(str.view(), long_str);
    EXPECT_EQ(str.size(), 10000u);
}

// ============================================================================
// UnsafeStringPool tests
// ============================================================================

TEST(UnsafeStringPoolTest, BasicIntern) {
    UnsafeStringPool pool;

    auto str1 = pool.intern("test");
    EXPECT_EQ(str1.view(), "test");
    EXPECT_EQ(pool.size(), 1u);

    auto str2 = pool.intern("test");
    EXPECT_EQ(str1, str2); // Should be same pointer
}

TEST(UnsafeStringPoolTest, Contains) {
    UnsafeStringPool pool;

    pool.intern("hello");

    EXPECT_TRUE(pool.contains("hello"));
    EXPECT_FALSE(pool.contains("world"));
}

TEST(UnsafeStringPoolTest, Clear) {
    UnsafeStringPool pool;

    (void)pool.intern("one");
    (void)pool.intern("two");

    EXPECT_EQ(pool.size(), 2u);

    pool.clear();

    EXPECT_EQ(pool.size(), 0u);
}

// ============================================================================
// Global string pool test
// ============================================================================

TEST(GlobalStringPoolTest, Singleton) {
    auto &pool1 = global_string_pool();
    auto &pool2 = global_string_pool();

    // Should be same instance
    EXPECT_EQ(&pool1, &pool2);
}

TEST(GlobalStringPoolTest, BasicUsage) {
    auto &pool = global_string_pool();

    // Clear first to ensure clean state
    pool.clear();

    auto str = pool.intern("global_test");
    EXPECT_EQ(str.view(), "global_test");
    EXPECT_TRUE(pool.contains("global_test"));

    pool.clear();
}

// ============================================================================
// InternedStringHash tests
// ============================================================================

TEST(InternedStringHashTest, HashConsistency) {
    StringPool pool;
    auto str1 = pool.intern("test");
    auto str2 = pool.intern("test");

    InternedStringHash hasher;
    EXPECT_EQ(hasher(str1), hasher(str2));
}

TEST(InternedStringHashTest, DifferentStringsHaveDifferentHashes) {
    StringPool pool;
    auto str1 = pool.intern("hello");
    auto str2 = pool.intern("world");

    InternedStringHash hasher;
    // Different strings should (almost certainly) have different hashes
    EXPECT_NE(hasher(str1), hasher(str2));
}
