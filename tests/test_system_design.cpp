#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cassert>
#include <thread>
#include <chrono>
#include "consistent_hash.hpp"
#include "rate_limiter.hpp"

using namespace std;
using namespace flashkv;

void test_consistent_hashing_distribution_and_churn() {
    cout << "[SYSTEM DESIGN TEST 1] Testing Consistent Hash Ring (Distribution & Migration)..." << endl;

    ConsistentHashRing ring(150); // 150 virtual nodes per server

    ring.add_node("Node-A");
    ring.add_node("Node-B");
    ring.add_node("Node-C");

    assert(ring.total_virtual_nodes() == 450);

    const int TOTAL_KEYS = 10000;
    map<string, string> initial_mapping;
    map<string, int> node_counts;

    // Distribute 10,000 keys across 3 nodes
    for (int i = 0; i < TOTAL_KEYS; ++i) {
        string key = "cache_key_user_" + to_string(i);
        string node = ring.get_node(key);
        initial_mapping[key] = node;
        node_counts[node]++;
    }

    cout << "  -> Initial Key Distribution across 3 nodes:" << endl;
    for (const auto& [node, count] : node_counts) {
        double pct = (count * 100.0) / TOTAL_KEYS;
        cout << "     * " << node << ": " << count << " keys (" << pct << "%)" << endl;
        // Verify balance: each node should get roughly 33% (between 20% and 48%)
        assert(pct >= 20.0 && pct <= 48.0);
    }

    // Now simulate HORIZONTAL SCALING: Add Node-D
    cout << "  -> Adding Node-D to ring (Horizontal Scaling to 4 nodes)..." << endl;
    ring.add_node("Node-D");

    int migrated_keys = 0;
    for (int i = 0; i < TOTAL_KEYS; ++i) {
        string key = "cache_key_user_" + to_string(i);
        string new_node = ring.get_node(key);
        if (new_node != initial_mapping[key]) {
            migrated_keys++;
        }
    }

    double migration_pct = (migrated_keys * 100.0) / TOTAL_KEYS;
    cout << "  -> Keys Migrated: " << migrated_keys << " / " << TOTAL_KEYS 
         << " (" << migration_pct << "%)" << endl;

    // In modulo hashing: ~75% of keys migrate.
    // In consistent hashing: only ~1/4 (~25% +/- 5%) of keys migrate!
    assert(migration_pct >= 18.0 && migration_pct <= 32.0);
    cout << "  -> Passed! (Consistent hashing preserved ~75% of cache keys!)" << endl;
}

void test_token_bucket_rate_limiter() {
    cout << "\n[SYSTEM DESIGN TEST 2] Testing Token Bucket Rate Limiter..." << endl;

    // Capacity: 5 tokens, Refill: 10 tokens per second (1 token every 100 ms)
    TokenBucketRateLimiter limiter(5.0, 10.0);
    string client_ip = "192.168.1.42";

    // Step 1: Burst of 5 requests should ALL be allowed
    for (int i = 1; i <= 5; ++i) {
        bool allowed = limiter.allow(client_ip);
        assert(allowed);
    }
    cout << "  -> Burst of 5 requests allowed successfully." << endl;

    // Step 2: 6th request immediately afterwards should be REJECTED (Rate Limited)
    bool rejected = limiter.allow(client_ip);
    assert(!rejected);
    cout << "  -> 6th immediate request REJECTED (HTTP 429 Too Many Requests)." << endl;

    // Step 3: Sleep 250ms (refills ~2.5 tokens)
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // Should now allow 2 requests
    assert(limiter.allow(client_ip));
    assert(limiter.allow(client_ip));
    // 3rd request should be rejected again
    assert(!limiter.allow(client_ip));

    cout << "  -> Passed! (Tokens accurately replenished on-demand after sleep)" << endl;
}

int main() {
    cout << "=================================================" << endl;
    cout << " FLASHKV MILESTONE 5: DISTRIBUTED SYSTEM DESIGN  " << endl;
    cout << "=================================================" << endl;

    test_consistent_hashing_distribution_and_churn();
    test_token_bucket_rate_limiter();

    cout << "=================================================" << endl;
    cout << "    ALL DISTRIBUTED SYSTEM DESIGN TESTS PASSED!  " << endl;
    cout << "=================================================" << endl;

    return 0;
}
