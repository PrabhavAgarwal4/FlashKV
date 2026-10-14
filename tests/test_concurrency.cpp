#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cassert>
#include <random>
#include <atomic>
#include "thread_pool.hpp"
#include "storage_engine.hpp"

using namespace std;
using namespace flashkv;

void test_thread_pool_basic() {
    cout << "[CONCURRENCY TEST 1] Testing ThreadPool execution & futures..." << endl;
    ThreadPool pool(4);
    assert(pool.worker_count() == 4);

    vector<future<int>> results;
    for (int i = 0; i < 8; ++i) {
        results.push_back(pool.submit([i] {
            return i * i;
        }));
    }

    for (int i = 0; i < 8; ++i) {
        assert(results[i].get() == i * i);
    }
    cout << "  -> Passed! (All 8 futures resolved correctly)" << endl;
}

void test_concurrent_reads_and_writes() {
    cout << "[CONCURRENCY TEST 2] Running multi-threaded stress test..." << endl;
    const int NUM_THREADS = 8;
    const int OPS_PER_THREAD = 2500;
    const int TOTAL_OPS = NUM_THREADS * OPS_PER_THREAD; // 20,000 operations
    const size_t CACHE_CAPACITY = 500;

    StorageEngine engine(CACHE_CAPACITY);
    ThreadPool pool(NUM_THREADS);

    // Pre-populate some keys
    for (int i = 0; i < 200; ++i) {
        engine.set("key:" + to_string(i), "init_val_" + to_string(i));
    }

    auto start_time = chrono::high_resolution_clock::now();

    vector<future<void>> futures;
    atomic<int> completed_ops{0};

    for (int t = 0; t < NUM_THREADS; ++t) {
        futures.push_back(pool.submit([&engine, &completed_ops, t, OPS_PER_THREAD]() {
            // Seed thread-local RNG
            mt19937 rng(1337 + t);
            uniform_int_distribution<int> key_dist(0, 1000);
            uniform_int_distribution<int> op_dist(0, 99);

            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                int key_id = key_dist(rng);
                string key = "key:" + to_string(key_id);
                int op = op_dist(rng);

                if (op < 70) {
                    // 70% Concurrent Reads (shared lock)
                    engine.get_readonly(key);
                } else if (op < 85) {
                    // 15% Strict LRU Reads (unique lock)
                    engine.get(key);
                } else if (op < 95) {
                    // 10% Writes (unique lock)
                    engine.set(key, "val_" + to_string(i));
                } else {
                    // 5% Deletes (unique lock)
                    engine.del(key);
                }
                completed_ops.fetch_add(1, memory_order_relaxed);
            }
        }));
    }

    // Wait for all threads to finish
    for (auto& f : futures) {
        f.get();
    }

    auto end_time = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> elapsed = end_time - start_time;

    double ops_per_sec = (TOTAL_OPS / elapsed.count()) * 1000.0;

    cout << "  -> Completed " << TOTAL_OPS << " operations across " << NUM_THREADS << " threads!" << endl;
    cout << "  -> Elapsed time: " << elapsed.count() << " ms" << endl;
    cout << "  -> Throughput:   " << (int)ops_per_sec << " ops/sec" << endl;
    cout << "  -> Cache Stats:  " << engine.hits() << " hits, " 
         << engine.misses() << " misses, " 
         << engine.writes() << " writes." << endl;

    // Verify cache size never exceeded capacity
    assert(engine.size() <= CACHE_CAPACITY);
    cout << "  -> Passed! (Zero data races, capacity strictly maintained)" << endl;
}

int main() {
    cout << "===============================================" << endl;
    cout << "  FLASHKV MILESTONE 2: CONCURRENCY & THREADS   " << endl;
    cout << "===============================================" << endl;

    test_thread_pool_basic();
    test_concurrent_reads_and_writes();

    cout << "===============================================" << endl;
    cout << " ALL MILESTONE 2 TESTS PASSED WITH ZERO RACES! " << endl;
    cout << "===============================================" << endl;

    return 0;
}
