# MiniRedis-CPP 🚀

A high-performance, cross-platform in-memory key-value database engine built from scratch in modern **C++**, featuring **RESP (REdis Serialization Protocol)** compliance, **LRU Cache Eviction**, **TTL (Time-To-Live)** expiration, **Sets & Hashes**, and **WAL (Write-Ahead Logging / AOF)** durability.

[![CI Pipeline](https://github.com/baadaldev/mini-redis-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/baadaldev/mini-redis-cpp/actions/workflows/ci.yml)
[![Language](https://img.shields.io/badge/Language-C%2B%2B14%2F17-00599C?logo=c%2B%2B)](https://isocpp.org/)
[![Protocol](https://img.shields.io/badge/Protocol-RESP%20Compliant-red?logo=redis)](https://redis.io/docs/reference/protocol-spec/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-blue)](#-quick-start)
[![Tests](https://img.shields.io/badge/Tests-10%2F10%20Passed-brightgreen)](#-automated-testing)
[![Throughput](https://img.shields.io/badge/Throughput-35%2C000%2B%20ops%2Fsec-orange)](#-performance--benchmarks)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

---

## 🏛️ System Architecture

```
                                 [Clients]
           (redis-cli, Python redis-py, Node ioredis, WebSockets)
                                     │
                                     │  TCP Sockets (RESP Protocol)
                                     ▼
 ┌────────────────────────────────────────────────────────────────────────┐
 │                           MiniRedis Server                             │
 │                                                                        │
 │  ┌──────────────────────────────────────────────────────────────────┐  │
 │  │ 1. Cross-Platform Network Layer (Winsock2 & POSIX Berkeley TCP)  │  │
 │  └──────────────────────────────────┬───────────────────────────────┘  │
 │                                     ▼                                  │
 │  ┌──────────────────────────────────────────────────────────────────┐  │
 │  │ 2. RESP Parser & Serializer (Array, Bulk String, Inline text)    │  │
 │  └──────────────────────────────────┬───────────────────────────────┘  │
 │                                     ▼                                  │
 │  ┌──────────────────────────────────────────────────────────────────┐  │
 │  │ 3. In-Memory Storage Engine                                      │  │
 │  │    • Strings & Atomic Integers (INCR/DECR/MGET/MSET)             │  │
 │  │    • Doubly Linked Lists (LPUSH/RPUSH/LRANGE)                    │  │
 │  │    • Hash Tables (HSET/HGET/HGETALL/HDEL)                        │  │
 │  │    • Unordered Sets (SADD/SMEMBERS/SISMEMBER/SREM/SCARD)         │  │
 │  │    • Lock-Safe Mutex & LockGuard Concurrency (Win32 & POSIX)     │  │
 │  └──────────────────┬───────────────────────────────┬───────────────┘  │
 │                     ▼                               ▼                  │
 │  ┌────────────────────────────────────┐ ┌───────────────────────────┐  │
 │  │ 4. Memory Management               │ │ 5. Durability Layer (WAL) │  │
 │  │    • TTL Active/Passive Cleaner    │ │    • Append-Only File     │  │
 │  │    • LRU (Least Recently Used)     │ │    • Atomic Crash Recovery│  │
 │  │      Eviction Policy               │ │    • AOF Log Compaction   │  │
 │  └────────────────────────────────────┘ └─────────────┬─────────────┘  │
 └───────────────────────────────────────────────────────┼────────────────┘
                                                         ▼
                                                  [Disk / .aof File]
```

---

## ✨ Key Features

* **Sub-Millisecond In-Memory Store:** $O(1)$ key lookup and mutation using hash tables with lock-safe multi-client concurrency.
* **Full RESP Compatibility:** Speaks the standard Redis Serialization Protocol. Connect using official `redis-cli`, Python `redis`, Node `ioredis`, or plain TCP sockets.
* **Rich Data Structures:**
  * **Strings & Counters:** `SET`, `GET`, `MSET`, `MGET`, `INCR`, `DECR`, `INCRBY`, `DECRBY`.
  * **Lists:** `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`.
  * **Hashes:** `HSET`, `HGET`, `HMSET`, `HMGET`, `HDEL`, `HEXISTS`, `HLEN`, `HGETALL`, `HKEYS`, `HVALS`.
  * **Sets:** `SADD`, `SMEMBERS`, `SISMEMBER`, `SREM`, `SCARD`.
  * **Key Inspection:** `TYPE`, `EXISTS`, `DEL`, `KEYS`, `DBSIZE`.
* **Cross-Platform Compatibility:** Native support for **Windows** (Winsock2) and **Linux / macOS** (POSIX Berkeley Sockets & pthreads).
* **Memory Management & LRU Eviction:** Configurable memory limits. Automatically evicts the least recently accessed keys when maximum capacity is reached.
* **Active & Passive TTL (Time-To-Live):** Keys expire automatically with `EXPIRE` and `TTL`. Expired keys are lazily cleaned on access and actively purged via a background thread.
* **Durability via Write-Ahead Logging (WAL / AOF):** Every mutating command is persisted sequentially to disk. On reboot or crash, the server automatically recovers its full in-memory state.
* **AOF Compaction:** Supports `BGREWRITEAOF` to compact the log file and eliminate stale, overwritten, or deleted keys.

---

## ⚡ Performance & Benchmarks

Benchmarked locally using `client/benchmark.py` over loopback TCP:

| Operation | Total Ops | Execution Time | Throughput (Ops/Sec) | Avg Latency |
| :--- | :--- | :--- | :--- | :--- |
| **PING** | 2,000 | 0.056s | **35,409.6 ops/sec** | **0.028 ms** (28 µs) |
| **GET** | 2,000 | 0.066s | **30,338.8 ops/sec** | **0.033 ms** (33 µs) |
| **INCR** | 2,000 | 0.122s | **16,371.4 ops/sec** | **0.061 ms** (61 µs) |
| **SET (with WAL sync)** | 2,000 | 0.161s | **12,390.8 ops/sec** | **0.081 ms** (81 µs) |

---

## 📋 Supported Command Reference

| Category | Commands | Description |
| :--- | :--- | :--- |
| **Strings** | `SET key value [EX sec] [PX ms]` | Store string value with optional expiration |
| | `GET key` | Retrieve string value |
| | `MSET key val [key val ...]` | Set multiple keys to multiple values |
| | `MGET key [key ...]` | Get values of all specified keys |
| | `DEL key [key2 ...]` | Delete one or more keys |
| | `EXISTS key [key2 ...]` | Check if key(s) exist |
| | `TYPE key` | Return key type (`string`, `list`, `hash`, `set`, `none`) |
| **Counters** | `INCR key` / `DECR key` | Atomically increment / decrement integer value |
| | `INCRBY key delta` / `DECRBY key delta` | Increment / decrement by arbitrary integer step |
| **Lists** | `LPUSH key val [val ...]` / `RPUSH` | Insert one or multiple elements at head or tail |
| | `LPOP key` / `RPOP key` | Remove and return element from head or tail |
| | `LRANGE key start stop` | Slice list elements (supports negative indexing) |
| **Hashes** | `HSET key field val [f v ...]` | Set one or multiple field/value pairs in hash |
| | `HGET key field` | Retrieve field value from hash |
| | `HMSET` / `HMGET` | Set or get multiple hash fields at once |
| | `HDEL key field [f ...]` | Delete one or more fields from hash |
| | `HEXISTS key field` | Check if field exists in hash |
| | `HLEN key` | Get total count of fields in hash |
| | `HGETALL key` | Return all fields and values in hash |
| | `HKEYS` / `HVALS` | Return all field names or values in hash |
| **Sets** | `SADD key member [mem ...]` | Add one or multiple members to a set |
| | `SMEMBERS key` | Return all members of the set |
| | `SISMEMBER key member` | Determine if a member exists in the set |
| | `SREM key member [mem ...]` | Remove one or more members from a set |
| | `SCARD key` | Return the number of elements in the set |
| **TTL** | `EXPIRE key seconds` | Set timeout on key |
| | `TTL key` | Return remaining time to live in seconds |
| **Persistence** | `BGREWRITEAOF` | Compact and rewrite append-only log file |
| **Server** | `PING [msg]`, `ECHO msg`, `INFO` | Connection health, echo message, server stats |
| | `DBSIZE`, `FLUSHALL` | Get active key count, wipe database clean |

---

## 🛠️ Project Structure

```text
mini-redis-cpp/
├── src/
│   ├── core/
│   │   ├── entry.hpp              # Value variant (String, List, Hash, Set) & TTL
│   │   ├── sync.hpp               # Cross-platform Mutex & LockGuard RAII (Win32 & POSIX)
│   │   ├── storage_engine.hpp     # In-memory storage with LRU, Sets, Hashes & TTL
│   │   └── storage_engine.cpp
│   ├── protocol/
│   │   ├── resp_parser.hpp        # RESP parser & serializer
│   │   └── resp_parser.cpp
│   ├── persistence/
│   │   ├── wal.hpp                # Write-Ahead Logging & crash recovery
│   │   └── wal.cpp
│   ├── network/
│   │   ├── server.hpp             # Cross-platform TCP server (Winsock & BSD Sockets)
│   │   └── server.cpp
│   └── main.cpp                   # CLI parsing, banner, & graceful shutdown
├── tests/
│   └── test_engine.cpp            # 10-phase comprehensive automated test suite
├── client/
│   ├── test_client.py             # Live TCP integration tests
│   └── benchmark.py               # Throughput & latency benchmark
├── .github/
│   └── workflows/
│       └── ci.yml                 # Matrix CI Pipeline (Linux & Windows)
├── Makefile                       # Unix / macOS build script
├── CMakeLists.txt                 # Modern CMake build definition
├── build.bat                      # Windows build script
└── README.md
```

---

## 🚀 Quick Start

### 1. Build Server & Run Unit Tests

#### On Linux / macOS:
```bash
make all
./test_suite
```

#### On Windows (using `build.bat`):
```cmd
build.bat
```

#### Using CMake (Cross-Platform):
```bash
cmake -B build
cmake --build build
```

---

### 2. Start the Server

```bash
# Start server on default port 6379 with WAL persistence
./mini_redis --port 6379 --aof data.aof

# On Windows:
.\mini_redis.exe --port 6379 --aof data.aof
```

---

### 3. Connect via Python or redis-cli

#### Interactive `redis-cli`:
```bash
redis-cli -p 6379

127.0.0.1:6379> SADD skills "C++" "Redis" "Systems"
(integer) 3
127.0.0.1:6379> SISMEMBER skills "C++"
(integer) 1
127.0.0.1:6379> SMEMBERS skills
1) "C++"
2) "Redis"
3) "Systems"
127.0.0.1:6379> TYPE skills
+set
127.0.0.1:6379> MSET lang1 "C++" lang2 "Python"
+OK
127.0.0.1:6379> MGET lang1 lang2
1) "C++"
2) "Python"
```

#### Automated Client Tests:
```bash
python client/test_client.py 6379
```

#### Run Performance Benchmark:
```bash
python client/benchmark.py 6379 2000
```

---

## ⚙️ Configuration & CLI Flags

| Flag | Default | Description |
| :--- | :--- | :--- |
| `--port <num>` | `6379` | TCP port number to listen on |
| `--aof <file>` | `data.aof` | Path to the Append-Only File for WAL persistence |
| `--maxkeys <num>` | `0` (unlimited) | Maximum key limit before LRU eviction triggers |

---

## 🧪 Automated Testing

The automated test suite verifies 10 critical database subsystems:
1. **Basic CRUD:** `SET`, `GET`, `DEL`, `EXISTS` semantics.
2. **Numeric Increment:** Type checking and atomicity on `INCRBY` / `DECRBY`.
3. **TTL & Expiry:** Millisecond accuracy and auto-cleanup.
4. **LRU Cache Eviction:** Capacity limits and eviction of least recently accessed keys.
5. **List Operations:** Negative indexing, `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`.
6. **Hash Operations:** `HSET`, `HGET`, `HMSET`, `HMGET`, `HDEL`, `HEXISTS`, `HLEN`, `HGETALL`, `HKEYS`, `HVALS`.
7. **Set Operations:** `SADD`, `SMEMBERS`, `SISMEMBER`, `SREM`, `SCARD`.
8. **Type & Multi-Operations:** `TYPE`, `MSET`, `MGET`.
9. **RESP Parser:** Verification of Arrays, Bulk Strings, Inlines, Errors, and Integers.
10. **WAL Durability:** Simulated crash and 100% state recovery from append-only logs.

---

## 📜 License

Distributed under the **MIT License**. Created by [Md Rakibul Islam (Baadal)](https://github.com/baadaldev).
