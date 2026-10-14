#pragma once

#include <string>
#include <shared_mutex>
#include <mutex>
#include <optional>
#include <atomic>
#include <cstdint>
#include "lru_cache.hpp"

namespace flashkv {

using std::string;
using std::shared_mutex;
using std::shared_lock;
using std::unique_lock;
using std::optional;
using std::atomic;
using std::uint64_t;
using std::size_t;

/**
 * @brief Thread-safe Storage Engine protecting the LRU cache with
 * Reader-Writer locking (std::shared_mutex).
 * 
 * Concurrency Model:
 * - Shared Lock (Concurrent Readers): Multiple threads can query keys simultaneously.
 * - Unique Lock (Exclusive Writer): Writes (set, del, clear) require exclusive ownership.
 * - Lock-free atomic counters for engine performance metrics.
 */
class StorageEngine {
public:
    explicit StorageEngine(size_t capacity)
        : cache_(capacity), hits_(0), misses_(0), writes_(0) {}

    // Non-copyable due to mutex
    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;

    /**
     * @brief Writes a key-value pair under an exclusive lock.
     * @return optional<string> Key that was evicted, if any.
     */
    optional<string> set(const string& key, string value) {
        unique_lock<shared_mutex> lock(rw_mutex_);
        writes_.fetch_add(1, std::memory_order_relaxed);
        return cache_.put(key, std::move(value));
    }

    /**
     * @brief Reads a value and updates its LRU recency position.
     * Requires unique_lock because it mutates the internal list pointers.
     */
    optional<string> get(const string& key) {
        unique_lock<shared_mutex> lock(rw_mutex_);
        auto result = cache_.get(key);
        if (result.has_value()) {
            hits_.fetch_add(1, std::memory_order_relaxed);
        } else {
            misses_.fetch_add(1, std::memory_order_relaxed);
        }
        return result;
    }

    /**
     * @brief High-throughput concurrent read.
     * Uses shared_lock (Reader lock). Multiple threads can execute this concurrently.
     * Does NOT update LRU recency to prevent data races.
     */
    optional<string> get_readonly(const string& key) const {
        shared_lock<shared_mutex> lock(rw_mutex_);
        auto result = cache_.peek(key);
        if (result.has_value()) {
            hits_.fetch_add(1, std::memory_order_relaxed);
        } else {
            misses_.fetch_add(1, std::memory_order_relaxed);
        }
        return result;
    }

    /**
     * @brief Deletes a key under an exclusive lock.
     */
    bool del(const string& key) {
        unique_lock<shared_mutex> lock(rw_mutex_);
        writes_.fetch_add(1, std::memory_order_relaxed);
        return cache_.remove(key);
    }

    /**
     * @brief Checks existence under a concurrent shared lock.
     */
    bool contains(const string& key) const {
        shared_lock<shared_mutex> lock(rw_mutex_);
        return cache_.contains(key);
    }

    /**
     * @brief Current number of items in storage.
     */
    size_t size() const {
        shared_lock<shared_mutex> lock(rw_mutex_);
        return cache_.size();
    }

    /**
     * @brief Total capacity of the storage.
     */
    size_t capacity() const {
        return cache_.capacity();
    }

    /**
     * @brief Metrics for performance monitoring.
     */
    uint64_t hits() const { return hits_.load(std::memory_order_relaxed); }
    uint64_t misses() const { return misses_.load(std::memory_order_relaxed); }
    uint64_t writes() const { return writes_.load(std::memory_order_relaxed); }

    void clear() {
        unique_lock<shared_mutex> lock(rw_mutex_);
        cache_.clear();
    }

private:
    mutable shared_mutex rw_mutex_;
    LRUCache<string, string> cache_;

    // Lock-free atomic metrics
    mutable atomic<uint64_t> hits_;
    mutable atomic<uint64_t> misses_;
    atomic<uint64_t> writes_;
};

} // namespace flashkv
