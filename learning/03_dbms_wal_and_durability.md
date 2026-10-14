# Learning Note 03: DBMS Durability, Write-Ahead Logging (WAL), and Crash Recovery

This document explains the database storage and persistence decisions implemented in Milestone 3.

---

## 1. WHY Write-Ahead Logging instead of Directly Saving the Database to Disk?

### The Question:
When a client sets a key, why don't we immediately update the database data file on disk instead of writing a separate log file?

### The Deep Answer:
It comes down to **Sequential vs. Random Disk I/O**:

1. **Random Disk Writes are Slow:**
   - If you write data directly to its final spot in a database file or B+ Tree page on disk, the disk drive must seek to random sector addresses.
   - On HDDs, this requires physical disk head movement (~5-10 ms per write).
   - On SSDs, random writes cause write amplification and block erase cycles.
   - Doing random disk I/O on every request drops throughput from 100,000 ops/sec down to ~100 ops/sec.

2. **Sequential Append-Only Writes are Blisteringly Fast:**
   - Appending a line to the end of an open file (`wal.log`) is **sequential I/O**.
   - The OS and disk controller write to contiguous blocks without seeking.
   - Both HDDs and SSDs handle sequential append writes at hundreds of megabytes per second.

---

## 2. WHY `std::ofstream::flush()` Matters (The OS Page Cache Trap)

### The Question:
When you write `file << "SET key val\n";`, is that data actually on the disk?

### The Deep Answer:
**NO!** 
1. When your C++ program writes to an `ofstream`, the data is first placed in the **C++ user-space stream buffer**.
2. When that buffer fills, the OS copies it into the **OS Page Cache** (in the kernel's RAM).
3. The OS kernel waits and writes it to the physical disk later in batches (dirty page flushing).
4. **The Trap:** If power is cut while the data is sitting in the OS Page Cache, the data is **permanently lost** even though your C++ code thought it wrote to disk!
5. **The Fix:** We call `file.flush()` (or `fsync()` at the OS level) to force the stream buffer to flush immediately through to durable storage.

---

## 3. HOW Crash Recovery Works (The Replay Loop)

### The Question:
How does a database recover its exact state after an unexpected kill/crash?

### The Deep Answer:
FlashKV follows the **ARIES Recovery Protocol** principle:
1. When the server boots up, it checks if `wal.log` exists.
2. If it does, the recovery manager opens the file in read mode.
3. It reads line by line:
   - Line: `SET user:1 Alice` $\to$ Calls `engine.set("user:1", "Alice")`
   - Line: `SET user:2 Bob`   $\to$ Calls `engine.set("user:2", "Bob")`
   - Line: `DEL user:1`       $\to$ Calls `engine.del("user:1")`
4. When EOF (End Of File) is reached, the in-memory cache has been rebuilt to the exact state it was in at the instant of the crash.
5. The server then opens the file in append mode (`ios::app`) to begin logging new live queries.
