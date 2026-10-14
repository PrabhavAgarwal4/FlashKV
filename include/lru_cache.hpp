#pragma once

#include <unordered_map>
#include <list>
#include <optional>
#include <utility>
#include <cstddef>

namespace flashkv {

using std::list;
using std::unordered_map;
using std::optional;
using std::pair;
using std::nullopt;
using std::size_t;

/**
 * @brief Thread-compatible O(1) Least-Recently-Used (LRU) Cache.
 * 
 * Data Structure:
 * - std::list (Doubly-Linked List): Maintains the recency order of entries.
 *   - Head (begin): Most Recently Used (MRU)
 *   - Tail (end - 1): Least Recently Used (LRU)
 * - std::unordered_map: Maps keys directly to list iterators for O(1) lookup.
 */
template <typename Key, typename Value>
class LRUCache {
public:
    using KeyValuePair = pair<Key, Value>;
    using ListIterator = typename list<KeyValuePair>::iterator;

    explicit LRUCache(size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            capacity_ = 1; // Minimum capacity guard
        }
    }

    /**
     * @brief Inserts or updates a key-value pair in O(1) time.
     * If the cache exceeds capacity, the LRU item at the tail is evicted.
     * 
     * @return std::optional<Key> Returns the evicted key if eviction occurred, else nullopt.
     */
    optional<Key> put(const Key& key, Value value) {
        auto it = map_.find(key);

        if (it != map_.end()) {
            // Key exists: update value and move node to head (MRU)
            it->second->second = std::move(value);
            list_.splice(list_.begin(), list_, it->second);
            return nullopt;
        }

        optional<Key> evicted_key = nullopt;

        // If capacity reached, evict Least Recently Used (tail)
        if (list_.size() >= capacity_) {
            evicted_key = list_.back().first;
            map_.erase(*evicted_key);
            list_.pop_back();
        }

        // Insert new element at head (MRU)
        list_.emplace_front(key, std::move(value));
        map_[key] = list_.begin();

        return evicted_key;
    }

    /**
     * @brief Retrieves a value by key in O(1) time.
     * Promotes the accessed key to the head (MRU).
     */
    optional<Value> get(const Key& key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return nullopt;
        }

        // Move the accessed node to front (MRU) using splice in O(1)
        // splice relinks pointers without allocating or freeing memory
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    /**
     * @brief Removes a key-value pair in O(1) time.
     * @return true if found and removed, false otherwise.
     */
    bool remove(const Key& key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return false;
        }

        list_.erase(it->second);
        map_.erase(it);
        return true;
    }

    /**
     * @brief Peeks at a value without changing its recency order.
     * Safe for concurrent read-only access.
     */
    optional<Value> peek(const Key& key) const {
        auto it = map_.find(key);
        if (it == map_.end()) {
            return nullopt;
        }
        return it->second->second;
    }

    /**
     * @brief Checks if a key exists without changing recency order.
     */
    bool contains(const Key& key) const {
        return map_.find(key) != map_.end();
    }

    /**
     * @brief Current number of items stored.
     */
    size_t size() const {
        return list_.size();
    }

    /**
     * @brief Maximum capacity of the cache.
     */
    size_t capacity() const {
        return capacity_;
    }

    /**
     * @brief Clears all items from the cache.
     */
    void clear() {
        map_.clear();
        list_.clear();
    }

private:
    size_t capacity_;
    list<KeyValuePair> list_;
    unordered_map<Key, ListIterator> map_;
};

} // namespace flashkv
