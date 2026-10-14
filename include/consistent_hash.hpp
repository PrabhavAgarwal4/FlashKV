#pragma once

#include <string>
#include <string_view>
#include <map>
#include <vector>
#include <mutex>
#include <cstdint>
#include <algorithm>

namespace flashkv {

using std::string;
using std::string_view;
using std::map;
using std::vector;
using std::mutex;
using std::unique_lock;
using std::uint32_t;
using std::size_t;

/**
 * @brief 32-bit FNV-1a Hash function.
 * Produces a uniform distribution across the 0 to 2^32 - 1 integer circle.
 */
inline uint32_t fnv1a_32(string_view text) {
    uint32_t hash = 2166136261u;
    for (char c : text) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    // Murmur3 32-bit avalanche finalizer to prevent bit clustering
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;
    return hash;
}

/**
 * @brief Consistent Hash Ring with Virtual Node support.
 * 
 * Provides O(log N) node lookup via std::map::lower_bound (Red-Black Tree),
 * minimizing key migration during horizontal scaling.
 */
class ConsistentHashRing {
public:
    explicit ConsistentHashRing(size_t virtual_nodes = 100) 
        : virtual_nodes_(virtual_nodes) {
        if (virtual_nodes_ == 0) virtual_nodes_ = 1;
    }

    /**
     * @brief Adds a physical node and creates its virtual replicas on the ring.
     */
    void add_node(const string& node) {
        unique_lock<mutex> lock(ring_mutex_);
        for (size_t i = 0; i < virtual_nodes_; ++i) {
            string vnode_id = node + "#" + std::to_string(i);
            uint32_t hash_pos = fnv1a_32(vnode_id);
            ring_[hash_pos] = node;
        }
    }

    /**
     * @brief Removes a physical node and purges all its virtual replicas.
     */
    void remove_node(const string& node) {
        unique_lock<mutex> lock(ring_mutex_);
        for (size_t i = 0; i < virtual_nodes_; ++i) {
            string vnode_id = node + "#" + std::to_string(i);
            uint32_t hash_pos = fnv1a_32(vnode_id);
            ring_.erase(hash_pos);
        }
    }

    /**
     * @brief Resolves which physical node owns a given key in O(log M) time.
     * Searches clockwise along the ring.
     */
    string get_node(string_view key) const {
        unique_lock<mutex> lock(ring_mutex_);
        if (ring_.empty()) {
            return "";
        }

        uint32_t key_hash = fnv1a_32(key);

        // Binary search for the first node with hash >= key_hash
        auto it = ring_.lower_bound(key_hash);

        // If beyond the end of the ring, wrap around clockwise to the start
        if (it == ring_.end()) {
            it = ring_.begin();
        }

        return it->second;
    }

    size_t total_virtual_nodes() const {
        unique_lock<mutex> lock(ring_mutex_);
        return ring_.size();
    }

    void clear() {
        unique_lock<mutex> lock(ring_mutex_);
        ring_.clear();
    }

private:
    size_t virtual_nodes_;
    map<uint32_t, string> ring_;
    mutable mutex ring_mutex_;
};

} // namespace flashkv
