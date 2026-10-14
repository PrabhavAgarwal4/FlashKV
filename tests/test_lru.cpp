#include <iostream>
#include <string>
#include <cassert>
#include "lru_cache.hpp"

using namespace std;
using namespace flashkv;

void test_basic_put_and_get() {
    cout << "[TEST 1] Running basic put and get test..." << endl;
    LRUCache<string, string> cache(3);

    cache.put("user:1", "Alice");
    cache.put("user:2", "Bob");

    assert(cache.size() == 2);
    assert(cache.get("user:1").value_or("") == "Alice");
    assert(cache.get("user:2").value_or("") == "Bob");
    assert(!cache.get("user:3").has_value());
    cout << "  -> Passed!" << endl;
}

void test_eviction_policy() {
    cout << "[TEST 2] Running eviction test (Capacity: 3)..." << endl;
    LRUCache<string, string> cache(3);

    cache.put("k1", "v1");
    cache.put("k2", "v2");
    cache.put("k3", "v3");

    // Cache is now full: [k3 (MRU), k2, k1 (LRU)]
    // Adding k4 should evict k1
    auto evicted = cache.put("k4", "v4");
    assert(evicted.has_value() && *evicted == "k1");
    assert(!cache.contains("k1"));
    assert(cache.contains("k2"));
    assert(cache.contains("k3"));
    assert(cache.contains("k4"));
    cout << "  -> Passed!" << endl;
}

void test_access_refresh() {
    cout << "[TEST 3] Running access refresh test (get promotes to MRU)..." << endl;
    LRUCache<string, string> cache(3);

    cache.put("k1", "v1");
    cache.put("k2", "v2");
    cache.put("k3", "v3");
    // State: [k3 (MRU), k2, k1 (LRU)]

    // Access k1 -> moves k1 to head
    // State becomes: [k1 (MRU), k3, k2 (LRU)]
    cache.get("k1");

    // Now inserting k4 should evict k2, NOT k1!
    auto evicted = cache.put("k4", "v4");
    assert(evicted.has_value() && *evicted == "k2");
    assert(cache.contains("k1")); // k1 was preserved because it was refreshed!
    assert(!cache.contains("k2")); // k2 was evicted
    assert(cache.contains("k3"));
    assert(cache.contains("k4"));
    cout << "  -> Passed!" << endl;
}

void test_update_existing_key() {
    cout << "[TEST 4] Running update existing key test..." << endl;
    LRUCache<string, string> cache(2);

    cache.put("k1", "original");
    cache.put("k2", "second");

    // Update k1
    cache.put("k1", "updated");
    assert(cache.size() == 2);
    assert(cache.get("k1").value_or("") == "updated");

    // k1 is now MRU; adding k3 must evict k2
    auto evicted = cache.put("k3", "third");
    assert(evicted.has_value() && *evicted == "k2");
    assert(cache.contains("k1"));
    assert(!cache.contains("k2"));
    assert(cache.contains("k3"));
    cout << "  -> Passed!" << endl;
}

void test_remove() {
    cout << "[TEST 5] Running remove test..." << endl;
    LRUCache<string, string> cache(3);

    cache.put("k1", "v1");
    cache.put("k2", "v2");

    assert(cache.remove("k1") == true);
    assert(!cache.contains("k1"));
    assert(cache.size() == 1);
    assert(cache.remove("non_existing") == false);
    cout << "  -> Passed!" << endl;
}

int main() {
    cout << "==========================================" << endl;
    cout << "   FLASHKV MILESTONE 1: LRU CACHE TESTS   " << endl;
    cout << "==========================================" << endl;

    test_basic_put_and_get();
    test_eviction_policy();
    test_access_refresh();
    test_update_existing_key();
    test_remove();

    cout << "==========================================" << endl;
    cout << " ALL MILESTONE 1 UNIT TESTS PASSED (100%) " << endl;
    cout << "==========================================" << endl;

    return 0;
}
