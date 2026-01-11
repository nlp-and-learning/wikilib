/**
 * @file string_pool.cpp
 * @brief Implementation of string interning
 */

#include "wikilib/core/string_pool.h"

namespace wikilib {

// ============================================================================
// StringPool (thread-safe)
// ============================================================================

InternedString StringPool::intern(std::string_view str) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = strings_.find(str);
    if (it != strings_.end()) {
        return InternedString(&*it);
    }
    
    auto [inserted, success] = strings_.emplace(str);
    total_size_ += str.size();
    return InternedString(&*inserted);
}

bool StringPool::contains(std::string_view str) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return strings_.find(str) != strings_.end();
}

InternedString StringPool::find(std::string_view str) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = strings_.find(str);
    if (it != strings_.end()) {
        return InternedString(&*it);
    }
    return InternedString{};
}

size_t StringPool::size() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return strings_.size();
}

size_t StringPool::memory_usage() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_size_ + strings_.size() * sizeof(std::string);
}

void StringPool::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    strings_.clear();
    total_size_ = 0;
}

void StringPool::reserve(size_t count) {
    std::lock_guard<std::mutex> lock(mutex_);
    strings_.reserve(count);
}

// ============================================================================
// UnsafeStringPool (not thread-safe)
// ============================================================================

InternedString UnsafeStringPool::intern(std::string_view str) {
    auto it = strings_.find(str);
    if (it != strings_.end()) {
        return InternedString(&*it);
    }
    
    auto [inserted, success] = strings_.emplace(str);
    total_size_ += str.size();
    return InternedString(&*inserted);
}

bool UnsafeStringPool::contains(std::string_view str) const {
    return strings_.find(str) != strings_.end();
}

InternedString UnsafeStringPool::find(std::string_view str) const {
    auto it = strings_.find(str);
    if (it != strings_.end()) {
        return InternedString(&*it);
    }
    return InternedString{};
}

size_t UnsafeStringPool::size() const noexcept {
    return strings_.size();
}

size_t UnsafeStringPool::memory_usage() const noexcept {
    return total_size_ + strings_.size() * sizeof(std::string);
}

void UnsafeStringPool::clear() {
    strings_.clear();
    total_size_ = 0;
}

void UnsafeStringPool::reserve(size_t count) {
    strings_.reserve(count);
}

// ============================================================================
// Global pool
// ============================================================================

StringPool& global_string_pool() {
    static StringPool pool;
    return pool;
}

} // namespace wikilib
