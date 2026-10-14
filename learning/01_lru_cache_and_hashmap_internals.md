# Learning Note 01: LRU Cache & Hash Map Internals

This document explains the deep "WHYs" behind every design and algorithmic decision made in Milestone 1.

---

## 1. WHY do we return `std::optional<Key>` on `put()`?

### The Question:
In standard coding platform exercises (e.g., LeetCode #146), `put(key, value)` returns `void`. Why does FlashKV return `std::optional<Key>`?

### The Deep Answer:
FlashKV is a real-world storage engine, not an isolated algorithm. When the cache is full and an item is kicked out:
1. **Durability & Disk Synchronization (Milestone 3):** If we persist data via Write-Ahead Logging (WAL) or maintain a secondary index on disk, the storage manager must know *which* key was evicted from RAM.
2. **Eviction Metrics:** Production caches (like Redis or Memcached) track eviction rates to monitor if the allocated RAM capacity is sufficient.
3. **Why `std::optional` instead of a dummy value (like `""` or `-1`):**
   - In older C++, programmers used magic sentinel values (e.g., empty string `""` or `nullptr`).
   - If a client legitimately stores `""` as a valid key, sentinel values cause silent bugs.
   - `std::optional<T>` is modern C++'s type-safe way of saying: *"This value exists if an eviction happened; otherwise, it is `std::nullopt`."*

---

## 2. WHY do we use `list::splice()` instead of `erase()` + `push_front()`?

### The Question:
When a key is accessed, it needs to move to the front (Most Recently Used). Why not just erase the old node and push a new node at the head?

### The Deep Answer:
It comes down to **operating system memory allocation overhead**:

```cpp
// The Naive Way:
list_.erase(it->second);         // Calls 'delete' -> OS Heap Deallocation
list_.emplace_front(key, val);   // Calls 'new'    -> OS Heap Allocation
```

1. **Heap Allocation Overhead:** `new` and `delete` make calls to the memory allocator (glibc ptmalloc / Windows Heap). Under high concurrency (e.g., 50,000 requests/sec), continuously allocating and freeing memory on every single read causes severe lock contention in the OS allocator.
2. **Memory Fragmentation:** Constant allocations fragment the heap over long server uptimes.
3. **The `splice()` Solution:**
   ```cpp
   list_.splice(list_.begin(), list_, it->second);
   ```
   A doubly linked list node is simply a block of memory with two pointers: `prev` and `next`. 
   `splice()` does not destroy or create any nodes. It simply re-links 4 pointers in CPU registers:
   - Sets `it->second->prev->next = it->second->next`
   - Sets `it->second->next->prev = it->second->prev`
   - Links the node to the current head
   - Updates the list's head pointer
   
   **Cost:** Exactly 3-4 CPU clock cycles, **zero heap allocations**, strictly $O(1)$.

---

## 3. WHY does `std::unordered_map` have an $O(N)$ Worst-Case?

### The Question:
Hash tables are known for $O(1)$ operations. Why do interviewers say the worst-case is $O(N)$?

### The Deep Answer:
1. **Pigeonhole Principle:** The universe of possible string keys is infinite ($\infty$), while the internal bucket array of a hash table is finite (e.g., 16, 64, or 1024 buckets).
2. **Separate Chaining:** When two keys produce the same bucket index (`hash(key) % buckets`), `std::unordered_map` places them in a linked list inside that bucket.
3. **The Worst-Case Scenario:**
   - **Average Case:** Keys are uniformly distributed across buckets. Each bucket has $\le 1$ element $\to O(1)$ lookup.
   - **Worst Case:** If $N$ keys all collide into the **exact same bucket**, they form a linked list of length $N$. Looking up or inserting a key requires traversing all $N$ items sequentially $\to O(N)$ linear time.

---

## 4. WHAT is a HashDoS Attack?

### The Question:
How can an attacker exploit this $O(N)$ worst-case against a server?

### The Deep Answer:
If an attacker discovers or reverse-engineers the hash function used by a server:
1. The attacker precomputes thousands of strings (e.g., 50,000 keys) that are mathematically engineered to produce the **exact same hash bucket**.
2. The attacker sends these keys to the server in a flood of requests.
3. The server inserts them. Instead of $50,000$ $O(1)$ insertions, every insertion now scans the full linked list:
   $$\text{Operations} = 1 + 2 + 3 + \dots + 50{,}000 = \frac{50{,}000 \times 50{,}001}{2} \approx 1.25 \text{ Billion Operations!}$$
4. The server's CPU hits **100%**, threads freeze, and the service becomes completely unavailable to legitimate users. This is called a **Hash Denial of Service (HashDoS)** attack.

### How Production Systems Defend Against HashDoS:
1. **Randomized Hash Seeds (SipHash):** Python, Rust, and Redis use SipHash. When the server starts up, it generates a secret random 128-bit seed. Attackers cannot precompute collisions because they do not know the seed.
2. **Treeification:** Java 8's `HashMap` automatically converts a bucket from a linked list to a **Red-Black Tree** if a single bucket contains more than 8 elements. This guarantees a worst-case of $O(\log N)$ instead of $O(N)$.
