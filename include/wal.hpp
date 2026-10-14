#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <iostream>
#include "storage_engine.hpp"

namespace flashkv {

using std::string;
using std::ofstream;
using std::ifstream;
using std::stringstream;
using std::mutex;
using std::unique_lock;
using std::ios;
using std::size_t;

struct RecoveryStats {
    size_t replayed_operations{0};
    double elapsed_ms{0.0};
};

/**
 * @brief Write-Ahead Logging (WAL) Engine for ACID Durability.
 * 
 * Guarantees that mutating operations (SET, DEL, CLEAR) are flushed
 * to persistent disk storage BEFORE being applied to RAM.
 * 
 * Provides automated crash recovery by replaying the append-only log.
 */
class WriteAheadLog {
public:
    explicit WriteAheadLog(string filepath = "wal.log") 
        : filepath_(std::move(filepath)) {
        open_log();
    }

    ~WriteAheadLog() {
        close_log();
    }

    // Non-copyable
    WriteAheadLog(const WriteAheadLog&) = delete;
    WriteAheadLog& operator=(const WriteAheadLog&) = delete;

    /**
     * @brief Appends a SET operation to disk and flushes.
     */
    void append_set(const string& key, const string& value) {
        unique_lock<mutex> lock(wal_mutex_);
        if (log_file_.is_open()) {
            log_file_ << "SET " << key << " " << value << "\n";
            log_file_.flush(); // Force write through OS buffers to durable disk
        }
    }

    /**
     * @brief Appends a DEL operation to disk and flushes.
     */
    void append_del(const string& key) {
        unique_lock<mutex> lock(wal_mutex_);
        if (log_file_.is_open()) {
            log_file_ << "DEL " << key << "\n";
            log_file_.flush();
        }
    }

    /**
     * @brief Appends a CLEAR operation to disk and flushes.
     */
    void append_clear() {
        unique_lock<mutex> lock(wal_mutex_);
        if (log_file_.is_open()) {
            log_file_ << "CLEAR\n";
            log_file_.flush();
        }
    }

    /**
     * @brief Replays all recorded operations from disk into memory.
     * Restores state after an unexpected process crash.
     */
    RecoveryStats recover(StorageEngine& engine) {
        unique_lock<mutex> lock(wal_mutex_);
        RecoveryStats stats;

        if (!std::filesystem::exists(filepath_)) {
            return stats;
        }

        // Close append stream during read
        close_log();

        auto start = std::chrono::high_resolution_clock::now();

        ifstream reader(filepath_);
        if (!reader.is_open()) {
            open_log();
            return stats;
        }

        string line;
        while (std::getline(reader, line)) {
            if (line.empty()) continue;

            stringstream ss(line);
            string command;
            ss >> command;

            if (command == "SET") {
                string key, value;
                if (ss >> key) {
                    // Extract remainder of line as the value (allows strings with spaces)
                    string rest;
                    std::getline(ss, rest);
                    // Trim leading space if present
                    if (!rest.empty() && rest.front() == ' ') {
                        rest.erase(0, 1);
                    }
                    value = rest;
                    engine.set(key, value);
                    stats.replayed_operations++;
                }
            } else if (command == "DEL") {
                string key;
                if (ss >> key) {
                    engine.del(key);
                    stats.replayed_operations++;
                }
            } else if (command == "CLEAR") {
                engine.clear();
                stats.replayed_operations++;
            }
        }

        reader.close();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        stats.elapsed_ms = elapsed.count();

        // Re-open in append mode for future live writes
        open_log();
        return stats;
    }

    /**
     * @brief Clears the log file on disk.
     */
    void truncate() {
        unique_lock<mutex> lock(wal_mutex_);
        close_log();
        ofstream trunc(filepath_, ios::trunc | ios::out);
        trunc.close();
        open_log();
    }

    const string& filepath() const {
        return filepath_;
    }

private:
    void open_log() {
        if (!log_file_.is_open()) {
            log_file_.open(filepath_, ios::app | ios::out);
        }
    }

    void close_log() {
        if (log_file_.is_open()) {
            log_file_.flush();
            log_file_.close();
        }
    }

    string filepath_;
    ofstream log_file_;
    mutex wal_mutex_;
};

} // namespace flashkv
