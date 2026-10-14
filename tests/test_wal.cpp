#include <iostream>
#include <string>
#include <cassert>
#include <filesystem>
#include "storage_engine.hpp"
#include "wal.hpp"

using namespace std;
using namespace flashkv;

const string TEST_WAL_FILE = "test_wal_crash.log";

void cleanup_test_file() {
    if (filesystem::exists(TEST_WAL_FILE)) {
        filesystem::remove(TEST_WAL_FILE);
    }
}

void test_wal_write_and_recovery() {
    cleanup_test_file();
    cout << "[WAL TEST 1] Simulating active database writes with WAL..." << endl;

    const size_t CAPACITY = 2000;
    const int INSERT_COUNT = 1000;

    {
        // SCOPE 1: The running database process
        StorageEngine engine(CAPACITY);
        WriteAheadLog wal(TEST_WAL_FILE);

        for (int i = 0; i < INSERT_COUNT; ++i) {
            string key = "user:" + to_string(i);
            string val = "Profile data for user " + to_string(i);

            // WAL Rule: Log to disk FIRST, then update memory
            wal.append_set(key, val);
            engine.set(key, val);
        }

        // Update some keys
        wal.append_set("user:42", "Updated VIP Profile");
        engine.set("user:42", "Updated VIP Profile");

        // Delete some keys
        wal.append_del("user:10");
        engine.del("user:10");
        wal.append_del("user:99");
        engine.del("user:99");

        assert(engine.size() == INSERT_COUNT - 2);
        cout << "  -> Memory populated with " << engine.size() << " records." << endl;
        cout << "  -> SIMULATING PROCESS CRASH (Ctrl+C / Kill / Power loss)!" << endl;
        // Scope ends: 'engine' object is destroyed. RAM is wiped completely!
    }

    cout << "\n[WAL TEST 2] Recovering state on server reboot..." << endl;
    {
        // SCOPE 2: Server reboot. A brand-new empty engine!
        StorageEngine new_engine(CAPACITY);
        assert(new_engine.size() == 0); // Proof that RAM was completely empty

        WriteAheadLog wal(TEST_WAL_FILE);

        cout << "  -> Replaying WAL records into fresh memory..." << endl;
        RecoveryStats stats = wal.recover(new_engine);

        cout << "  -> Replayed " << stats.replayed_operations 
             << " operations in " << stats.elapsed_ms << " ms!" << endl;

        // Verify state restoration
        assert(new_engine.size() == INSERT_COUNT - 2);

        // Verify intact values
        auto val0 = new_engine.get("user:0");
        assert(val0.has_value() && *val0 == "Profile data for user 0");

        // Verify updated value
        auto val42 = new_engine.get("user:42");
        assert(val42.has_value() && *val42 == "Updated VIP Profile");

        // Verify deleted values are NOT restored
        assert(!new_engine.contains("user:10"));
        assert(!new_engine.contains("user:99"));

        cout << "  -> State verified: 100% data integrity restored after crash!" << endl;
    }

    cleanup_test_file();
}

int main() {
    cout << "===============================================" << endl;
    cout << "      FLASHKV MILESTONE 3: DBMS WAL TESTS      " << endl;
    cout << "===============================================" << endl;

    test_wal_write_and_recovery();

    cout << "===============================================" << endl;
    cout << "   ALL DBMS DURABILITY & CRASH TESTS PASSED!   " << endl;
    cout << "===============================================" << endl;

    return 0;
}
