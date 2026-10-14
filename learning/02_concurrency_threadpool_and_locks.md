# Learning Note 02: Concurrency, Thread Pools, and Reader-Writer Locks

This document details the core OS and concurrency design choices for Milestone 2.

---

## 1. WHY a Thread Pool instead of Spawning Threads per Request?

### The Question:
When a client sends a request to the server, why not simply write `std::thread t(handle_request, socket); t.detach();`?

### The Deep Answer:
1. **Thread Creation is Expensive:** Spawning an OS thread requires:
   - Entering kernel mode via an OS system call.
   - Allocating a thread execution stack (typically **1 MB** on Windows and Linux per thread).
   - Registering the thread in the OS scheduler.
2. **Context Switching Thrashing:**
   - If 5,000 clients connect at once, spawning 5,000 threads consumes ~5 GB of memory just for stacks.
   - The OS CPU scheduler spends more time swapping registers and cache lines between threads (**Context Switching Overhead**) than doing actual useful work.
3. **The Thread Pool Solution:**
   - We allocate a fixed number of worker threads (e.g., matching the machine's CPU core count: `std::thread::hardware_concurrency()`).
   - When a request arrives, it is enqueued into a task queue.
   - Workers pick up tasks, execute them, and immediately loop back to wait for the next task.
   - **Result:** Constant memory usage, zero thread creation during runtime, and maximum CPU cache locality.

---

## 2. WHY `std::condition_variable` with a Predicate (Spurious Wakeups)?

### The Question:
How does an idle worker thread wait for work without burning CPU?

### The Deep Answer:
If a worker thread did this:
```cpp
// BAD: Busy-waiting / Polling
while (task_queue.empty()) {
    // Keeps spinning!
}
```
The thread would run in a tight loop, pinning a CPU core to **100% usage** while doing nothing.

### The Correct Mechanism:
We put the thread to sleep using `std::condition_variable`:
```cpp
unique_lock<mutex> lock(queue_mutex);
cv.wait(lock, [this] { return stop_flag || !tasks.empty(); });
```
1. **Futex / Kernel Sleep:** `cv.wait()` puts the thread into an operating system wait state (`WAIT_OBJECT_0` on Windows, `futex` on Linux). The CPU uses **0% power**.
2. **Spurious Wakeups:** Operating system kernels (and POSIX specifications) allow a condition variable to wake up even if no signal was sent (e.g., due to OS interrupts or hardware signals).
3. **The Predicate Lambda:** By passing `[this] { return stop_flag || !tasks.empty(); }`, if a spurious wakeup happens, the thread automatically re-checks the condition. If the queue is still empty, it goes right back to sleep.

---

## 3. WHY `std::shared_mutex` (Reader-Writer Lock)?

### The Question:
Why not protect the storage engine with a simple `std::mutex`?

### The Deep Answer:
1. **Read-Heavy Workloads:** In caching systems (like Redis or Memcached), typically **85% to 95%** of all incoming queries are reads (`GET`), and only 5% to 15% are writes (`SET`, `DEL`).
2. **Lock Contention:** A standard `std::mutex` is strictly exclusive. If 16 worker threads want to read different keys at the same time, a regular mutex forces them to wait one by one in a single line, destroying parallelism.
3. **Reader-Writer Solution:**
   - `std::shared_lock<std::shared_mutex>`: Allows multiple threads to read **simultaneously** without blocking each other.
   - `std::unique_lock<std::shared_mutex>`: When a write arrives, it requests exclusive access, blocking all new reads until the write completes.

---

## 4. THE CORE LRU CONCURRENCY DILEMMA (Senior Placement Topic)

### The Dilemma:
* An LRU cache modifies its recency list on every `get()` (to move the accessed item to the head).
* Can multiple threads execute `list::splice()` concurrently under a `shared_lock`?
* **NO!** Modifying pointers concurrently without exclusive locking causes a **data race**, resulting in memory corruption and segmentation faults.

### How FlashKV Solves This:
FlashKV's `StorageEngine` provides two distinct read methods:
1. **`get(key)` (Strict LRU):** Acquires a `unique_lock` to safely update the recency order in memory.
2. **`get_readonly(key)` (High-Throughput Concurrent Read):** Acquires a `shared_lock` and reads the value **without** altering the recency list. 
   - Multiple threads can call `get_readonly()` simultaneously with zero lock contention.
   - In production systems (like Redis), LRU is often approximated using random clock sampling for this exact reason!
