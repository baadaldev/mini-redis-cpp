# MiniRedis-CPP 🚀
#test 10
#For my personal use

A high-performance, in-memory key-value database engine built from scratch in modern **C++**, featuring **RESP (REdis Serialization Protocol)** compliance, **LRU Cache Eviction**, **TTL (Time-To-Live)** expiration, and **WAL (Write-Ahead Logging / AOF)** durability.

[![Language](https://img.shields.io/badge/Language-C%2B%2B14%2F17-00599C?logo=c%2B%2B)](https://isocpp.org/)
[![Protocol](https://img.shields.io/badge/Protocol-RESP%20Compliant-red?logo=redis)](https://redis.io/docs/reference/protocol-spec/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20(Winsock2)-0078D6?logo=windows)](https://microsoft.com)
[![Tests](https://img.shields.io/badge/Tests-8%2F8%20Passed-brightgreen)](#automated-testing)
[![Throughput](https://img.shields.io/badge/Throughput-35%2C000%2B%20ops%2Fsec-orange)](#performance--benchmarks)

---

## 🏛️ System Architecture

```
                                 [Clients]
           (redis-cli, Python redis-py, Node ioredis, WebSockets)
                                     │
                                     │  TCP Sockets (RESP)
                                     ▼
 ┌────────────────────────────────────────────────────────────────────────┐
 │                           MiniRedis Server                             │
 │                                                                        │
 │  ┌──────────────────────────────────────────────────────────────────┐  │
 │  │ 1. Network Layer (Winsock2 TCP Multi-Client Concurrency)         │  │
 │  └──────────────────────────────────┬───────────────────────────────┘  │
 │                                     ▼                                  │
 │  ┌──────────────────────────────────────────────────────────────────┐  │
 │  │ 2. RESP Parser & Serializer (Array, Bulk String, Inline text)    │  │
 │  └──────────────────────────────────┬───────────────────────────────┘  │
 │                                     ▼                                  │
 │  ┌──────────────────────────────────────────────────────────────────┐  │
 │  │ 3. In-Memory Storage Engine                                      │  │
 │  │    • Strings & Atomic Integers (INCR/DECR)                       │  │
 │  │    • Doubly Linked Lists (LPUSH/RPUSH/LRANGE)                    │  │
 │  │    • High-Performance Win32 Mutex & LockGuard Thread-Safety      │  │
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

* **Sub-Millisecond In-Memory Store:** $O(1)$ key lookup and mutation using hash tables with lock-safe concurrency.
* **Full RESP Compatibility:** Seamlessly speaks the Redis Serialization Protocol. You can connect using official `redis-cli`, Python `redis`, Node `ioredis`, or plain TCP sockets.
* **Multi-Client Concurrency:** Non-blocking connection management handling concurrent client requests simultaneously.
* **Memory Management & LRU Eviction:** Configurable memory limits. Automatically evicts the least recently accessed keys when maximum capacity is reached.
* **Active & Passive TTL (Time-To-Live):** Keys expire automatically with `EXPIRE`, `TTL`, or `SETEX`. Expired keys are lazily cleaned on access and actively purged via a background thread.
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
| **Strings** | `SET key value [EX sec]` | Store string value with optional expiration |
| | `GET key` | Retrieve string value |
| | `DEL key [key2 ...]` | Delete one or more keys |
| | `EXISTS key [key2 ...]`| Check if key(s) exist |
| **Counters** | `INCR key` / `DECR key` | Atomically increment / decrement integer value |
| | `INCRBY key delta` | Increment by arbitrary integer step |
| **Lists** | `LPUSH` / `RPUSH` | Insert one or multiple elements at head or tail |
| | `LPOP` / `RPOP` | Remove and return element from head or tail |
| | `LRANGE key start stop`| Slice list elements (supports negative indexing) |
| **Hashes** | `HSET key f v [f v ...]`| Set one or multiple field/value pairs in hash |
| | `HGET key field` | Retrieve field value from hash |
| | `HMSET` / `HMGET` | Set or get multiple hash fields at once |
| | `HDEL key f [f ...]` | Delete one or more fields from hash |
| | `HEXISTS key field` | Check if field exists in hash |
| | `HLEN key` | Get total count of fields in hash |
| | `HGETALL key` | Return all fields and values in hash |
| | `HKEYS` / `HVALS` | Return all field names or values in hash |
| **TTL** | `EXPIRE key seconds` | Set timeout on key |
| | `TTL key` | Return remaining time to live in seconds |
| **Persistence** | `BGREWRITEAOF` | Asynchronously rewrite append-only log file |
| **Server** | `PING`, `ECHO`, `INFO` | Connection health, echo message, server stats |
| | `DBSIZE`, `FLUSHALL` | Get active key count, wipe database clean |

---

## 🛠️ Project Structure
```text
mini-redis-cpp/
├── src/
│   ├── core/
│   │   ├── entry.hpp              # Value variant representation & TTL metadata
│   │   ├── sync.hpp               # High-performance Win32 Mutex & LockGuard RAII
│   │   ├── storage_engine.hpp     # In-memory storage with LRU, Hashes & TTL
│   │   └── storage_engine.cpp
│   ├── protocol/
│   │   ├── resp_parser.hpp        # RESP parser & serializer
│   │   └── resp_parser.cpp
│   ├── persistence/
│   │   ├── wal.hpp                # Write-Ahead Logging & crash recovery
│   │   └── wal.cpp
│   ├── network/
│   │   ├── server.hpp             # Winsock2 TCP server & command dispatcher
│   │   └── server.cpp
│   └── main.cpp                   # CLI parsing, banner, & graceful shutdown
├── tests/
│   └── test_engine.cpp            # 8-phase automated test suite
├── client/
│   ├── test_client.py             # Live TCP integration tests
│   └── benchmark.py               # Throughput & latency benchmark
├── build.bat                      # One-click Windows compilation script
└── README.md
```

---

## 🚀 Quick Start

### 1. Build Server & Run Unit Tests
Clone the repository and run the build script:
```cmd
build.bat
```
This compiles `mini_redis.exe` and executes all 8 unit test suites.

### 2. Start the Server
```cmd
.\mini_redis.exe --port 6379 --aof data.aof
```

### 3. Connect via Python Client or redis-cli
You can connect using any standard Redis client or the included test client:
```cmd
python client/test_client.py 6379

```
## ⚙️ Configuration & CLI Flags
The server accepts several command-line flags to customize runtime behavior:
| Flag | Default | Description |
| :--- | :--- | :--- |
| `--port <num>` | `6379` | TCP port number to listen on |
| `--aof <file>` | `data.aof` | Path to the Append-Only File for WAL persistence |
| `--maxmemory <bytes>` | `100MB` | Maximum memory limit before LRU eviction triggers |
| `--sync-every-write` | `true` | Flush write-ahead log to disk synchronously on mutation |

Or connect via interactive `redis-cli`:
```bash
redis-cli -p 6379
127.0.0.1:6379> HSET user:100 name "Baadal" role "Engineer"
(integer) 2
127.0.0.1:6379> HGET user:100 name
"Baadal"
127.0.0.1:6379> HGETALL user:100
1) "name"
2) "Baadal"
3) "role"
4) "Engineer"
```

---

## 🧪 Automated Testing

The automated test suite verifies 8 critical database subsystems:
1. **Basic CRUD:** `SET`, `GET`, `DEL`, `EXISTS` semantics.
2. **Numeric Increment:** Type checking and atomicity on `INCRBY`.
3. **TTL & Expiry:** Millisecond accuracy and auto-cleanup.
4. **LRU Cache Eviction:** Capacity limits and eviction of least recently accessed keys.
5. **List Operations:** Negative indexing, `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`.
6. **Hash Operations:** `HSET`, `HGET`, `HMSET`, `HMGET`, `HDEL`, `HEXISTS`, `HLEN`, `HGETALL`, `HKEYS`, `HVALS`.
7. **RESP Parser:** Verification of Arrays, Bulk Strings, Inlines, Errors, and Integers.
8. **WAL Durability:** Simulated crash and 100% state recovery from append-only logs.

Run tests anytime:
```cmd
.\test.exe
```

---

## 📜 License
MIT License. Created by [Md Rakibul Islam (Baadal)](https://github.com/baadaldev).
