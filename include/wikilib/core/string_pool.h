#pragma once

/**
 * @file string_pool.hpp
 * @brief Efficient string interning and storage
 *
 * String pool provides deduplication and efficient storage for strings
 * that are frequently repeated (like template names, parameter names, etc.)
 */

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace wikilib {

/**
 * @brief Interned string handle
 *
 * Lightweight handle to an interned string. Comparison is O(1) pointer comparison.
 */
class InternedString {
public:
    InternedString() = default;

    [[nodiscard]] std::string_view view() const noexcept {
        return ptr_ ? *ptr_ : std::string_view{};
    }

    [[nodiscard]] const char *c_str() const noexcept {
        return ptr_ ? ptr_->c_str() : "";
    }

    [[nodiscard]] size_t size() const noexcept {
        return ptr_ ? ptr_->size() : 0;
    }

    [[nodiscard]] bool empty() const noexcept {
        return !ptr_ || ptr_->empty();
    }

    // Fast pointer comparison
    bool operator==(const InternedString &other) const noexcept {
        return ptr_ == other.ptr_;
    }

    bool operator!=(const InternedString &other) const noexcept {
        return ptr_ != other.ptr_;
    }

    // Comparison with regular strings
    bool operator==(std::string_view other) const noexcept {
        return view() == other;
    }

    operator std::string_view() const noexcept {
        return view();
    }

    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }
private:
    friend class StringPool;
    friend class UnsafeStringPool;

    explicit InternedString(const std::string *ptr) : ptr_(ptr) {
    }

    const std::string *ptr_ = nullptr;
};

/**
 * @brief Hash function for InternedString
 */
struct InternedStringHash {
    size_t operator()(const InternedString &s) const noexcept {
        return std::hash<const void *>{}(static_cast<const void *>(&*s.view().begin()));
    }
};

/**
 * @brief String pool for interning and deduplication
 *
 * Thread-safe by default. Use UnsafeStringPool for single-threaded scenarios.
 */
class StringPool {
public:
    StringPool() = default;
    ~StringPool() = default;

    // Non-copyable, movable
    StringPool(const StringPool &) = delete;
    StringPool &operator=(const StringPool &) = delete;
    StringPool(StringPool &&) noexcept = default;
    StringPool &operator=(StringPool &&) noexcept = default;

    /**
     * @brief Intern a string
     * @return Handle to the interned string
     *
     * If the string already exists in the pool, returns existing handle.
     * Thread-safe.
     */
    [[nodiscard]] InternedString intern(std::string_view str);

    /**
     * @brief Check if string is already interned
     */
    [[nodiscard]] bool contains(std::string_view str) const;

    /**
     * @brief Get interned string if it exists
     */
    [[nodiscard]] InternedString find(std::string_view str) const;

    /**
     * @brief Number of unique strings in pool
     */
    [[nodiscard]] size_t size() const noexcept;

    /**
     * @brief Total memory used by strings (approximate)
     */
    [[nodiscard]] size_t memory_usage() const noexcept;

    /**
     * @brief Clear all interned strings
     * @warning Invalidates all InternedString handles!
     */
    void clear();

    /**
     * @brief Reserve capacity for expected number of strings
     */
    void reserve(size_t count);
private:
    struct StringViewHash {
        using is_transparent = void;

        size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }

        size_t operator()(const std::string &s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    struct StringViewEqual {
        using is_transparent = void;

        bool operator()(const std::string &a, const std::string &b) const noexcept {
            return a == b;
        }

        bool operator()(const std::string &a, std::string_view b) const noexcept {
            return a == b;
        }

        bool operator()(std::string_view a, const std::string &b) const noexcept {
            return a == b;
        }
    };

    std::unordered_set<std::string, StringViewHash, StringViewEqual> strings_;
    mutable std::mutex mutex_;
    size_t total_size_ = 0;
};

/**
 * @brief Non-thread-safe string pool for single-threaded use
 *
 * Faster than StringPool when thread safety is not needed.
 */
class UnsafeStringPool {
public:
    UnsafeStringPool() = default;

    [[nodiscard]] InternedString intern(std::string_view str);
    [[nodiscard]] bool contains(std::string_view str) const;
    [[nodiscard]] InternedString find(std::string_view str) const;
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] size_t memory_usage() const noexcept;
    void clear();
    void reserve(size_t count);
private:
    struct StringViewHash {
        using is_transparent = void;

        size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }

        size_t operator()(const std::string &s) const noexcept {
            return std::hash<std::string_view>{}(s);
        }
    };

    struct StringViewEqual {
        using is_transparent = void;

        bool operator()(const std::string &a, const std::string &b) const noexcept {
            return a == b;
        }

        bool operator()(const std::string &a, std::string_view b) const noexcept {
            return a == b;
        }

        bool operator()(std::string_view a, const std::string &b) const noexcept {
            return a == b;
        }
    };

    std::unordered_set<std::string, StringViewHash, StringViewEqual> strings_;
    size_t total_size_ = 0;
};

/**
 * @brief Global string pool for commonly used strings
 *
 * Use for template names, namespace names, and other frequently repeated strings.
 */
StringPool &global_string_pool();

} // namespace wikilib
