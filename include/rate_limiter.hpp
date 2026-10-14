#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <algorithm>

namespace flashkv {

using std::string;
using std::unordered_map;
using std::mutex;
using std::unique_lock;
using std::min;
using Clock = std::chrono::steady_clock;

/**
 * @brief Thread-safe Token Bucket Rate Limiter.
 * 
 * Uses lazy time-delta calculation to refill tokens on-demand,
 * eliminating background timer threads while providing burst tolerance.
 */
class TokenBucketRateLimiter {
public:
    /**
     * @param capacity Maximum tokens the bucket can hold (Burst limit).
     * @param refill_rate Number of tokens replenished per second.
     */
    TokenBucketRateLimiter(double capacity, double refill_rate)
        : capacity_(capacity), refill_rate_(refill_rate) {}

    /**
     * @brief Evaluates whether a request from a client should be allowed.
     * @return true if allowed (token consumed), false if rate-limited.
     */
    bool allow(const string& client_id, double tokens_requested = 1.0) {
        unique_lock<mutex> lock(mutex_);
        auto now = Clock::now();

        auto it = buckets_.find(client_id);
        if (it == buckets_.end()) {
            // First request from this client: initialize with full capacity minus requested
            if (capacity_ >= tokens_requested) {
                buckets_[client_id] = {capacity_ - tokens_requested, now};
                return true;
            }
            return false;
        }

        Bucket& bucket = it->second;

        // Calculate time elapsed since last check
        std::chrono::duration<double> elapsed = now - bucket.last_update;
        bucket.last_update = now;

        // Refill tokens lazily based on elapsed time
        bucket.tokens = min(capacity_, bucket.tokens + elapsed.count() * refill_rate_);

        if (bucket.tokens >= tokens_requested) {
            bucket.tokens -= tokens_requested;
            return true;
        }

        return false; // Insufficient tokens -> Rejected!
    }

    /**
     * @brief Returns current available tokens for a client.
     */
    double tokens(const string& client_id) {
        unique_lock<mutex> lock(mutex_);
        auto it = buckets_.find(client_id);
        if (it == buckets_.end()) {
            return capacity_;
        }

        auto now = Clock::now();
        std::chrono::duration<double> elapsed = now - it->second.last_update;
        return min(capacity_, it->second.tokens + elapsed.count() * refill_rate_);
    }

    void clear() {
        unique_lock<mutex> lock(mutex_);
        buckets_.clear();
    }

private:
    struct Bucket {
        double tokens{0.0};
        Clock::time_point last_update;
    };

    double capacity_;
    double refill_rate_;
    unordered_map<string, Bucket> buckets_;
    mutex mutex_;
};

} // namespace flashkv
