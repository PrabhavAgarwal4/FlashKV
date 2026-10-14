# FlashKV

**FlashKV** is a high-performance, multi-threaded distributed in-memory cache and key-value storage engine built in modern C++ (C++17/C++20). It demonstrates core computer science systems concepts across Operating Systems, DBMS, Networking, Distributed Systems, and Data Structures.

---

## Key Features

- **Core Storage & Eviction:** Fast $O(1)$ key lookup and $O(1)$ Least-Recently-Used (LRU) cache eviction.
- **High Concurrency:** Reader-Writer locking (`std::shared_mutex`) and a custom Producer-Consumer Worker Thread Pool.
- **ACID Durability:** Write-Ahead Logging (WAL) engine guaranteeing crash recovery and zero data loss on unexpected restarts.
- **Client-Server Networking:** Custom TCP protocol parser with non-blocking socket handling over raw Winsock.
- **Distributed Sharding:** Consistent Hashing with virtual nodes for minimal key migration during cluster resizing.
- **Traffic Protection:** Token Bucket rate limiter per client IP.

---

## Project Structure

```
FlashKV/
├── .vscode/               # VS Code tasks and compiler configurations
├── include/               # C++ header files (.hpp)
│   ├── lru_cache.hpp      # O(1) LRU Cache interface and implementation
│   ├── storage_engine.hpp # Storage engine with Reader-Writer locking
│   ├── thread_pool.hpp    # Task queue and worker thread pool
│   ├── wal.hpp            # Write-Ahead Log engine
│   ├── server.hpp         # TCP socket server
│   ├── consistent_hash.hpp# Consistent Hash Ring with virtual nodes
│   └── rate_limiter.hpp   # Token bucket rate limiter
├── src/                   # C++ implementation files (.cpp)
├── tests/                 # Unit tests and benchmarks
├── docs/                  # Architecture and interview defense handbook
├── .gitignore
├── README.md
└── Makefile / build.bat   # Compilation scripts
```

---

## Documentation

For full architectural diagrams, CS concepts mapping, and placement interview Q&A, see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).
