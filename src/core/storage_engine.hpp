#ifndef STORAGE_ENGINE_HPP
#define STORAGE_ENGINE_HPP

#include "entry.hpp"
#include "sync.hpp"
#include <unordered_map>
#include <list>
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

class StorageEngine {
public:
    explicit StorageEngine(size_t max_keys = 0); // 0 means unlimited

    // String operations
    bool set(const std::string& key, const std::string& value, int64_t expire_ms = -1);
    bool get(const std::string& key, std::string& value);
    bool del(const std::string& key);
    bool exists(const std::string& key);

    // Numeric operations
    bool incrby(const std::string& key, int64_t delta, int64_t& new_value, std::string& err_msg);

    // Key expiry & TTL
    bool expire(const std::string& key, int64_t seconds);
    int64_t ttl(const std::string& key); // -2: not exists, -1: no expiry, >=0: remaining seconds

    // List operations
    bool lpush(const std::string& key, const std::vector<std::string>& values, size_t& new_length);
    bool rpush(const std::string& key, const std::vector<std::string>& values, size_t& new_length);
    bool lpop(const std::string& key, std::string& value);
    bool rpop(const std::string& key, std::string& value);
    bool lrange(const std::string& key, int64_t start, int64_t stop, std::vector<std::string>& results);

    // Hash operations
    bool hset(const std::string& key, const std::string& field, const std::string& value, int& added, std::string& err_msg);
    bool hget(const std::string& key, const std::string& field, std::string& value, bool& found, std::string& err_msg);
    bool hdel(const std::string& key, const std::vector<std::string>& fields, int& deleted_count, std::string& err_msg);
    bool hexists(const std::string& key, const std::string& field, bool& exists, std::string& err_msg);
    bool hlen(const std::string& key, size_t& length, std::string& err_msg);
    bool hgetall(const std::string& key, std::vector<std::pair<std::string, std::string>>& items, std::string& err_msg);
    bool hkeys(const std::string& key, std::vector<std::string>& keys, std::string& err_msg);
    bool hvals(const std::string& key, std::vector<std::string>& vals, std::string& err_msg);

    // Retrieval helpers for persistence & inspection
    bool get_hash(const std::string& key, std::unordered_map<std::string, std::string>& hash_map);
    bool get_list(const std::string& key, std::vector<std::string>& list);

    // Server management
    void flushall();
    size_t dbsize();
    std::vector<std::string> get_all_keys();

    // Background maintenance
    size_t purge_expired();

    // Helper: current time in ms
    static int64_t current_time_ms();

private:
    size_t max_keys_;
    mutable Mutex mutex_;
    
    // Key -> Entry
    std::unordered_map<std::string, Entry> store_;
    
    // LRU eviction list and iterators
    std::list<std::string> lru_list_;
    std::unordered_map<std::string, std::list<std::string>::iterator> lru_map_;

    // Internal helpers (called while holding lock)
    bool is_expired_locked(const std::string& key, int64_t now_ms);
    void touch_lru_locked(const std::string& key);
    void remove_key_locked(const std::string& key);
    void evict_if_needed_locked();
};

#endif // STORAGE_ENGINE_HPP
