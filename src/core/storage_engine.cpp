#include "storage_engine.hpp"
#include <algorithm>
#include <sstream>

StorageEngine::StorageEngine(size_t max_keys) : max_keys_(max_keys) {}

int64_t StorageEngine::current_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

bool StorageEngine::is_expired_locked(const std::string& key, int64_t now_ms) {
    auto it = store_.find(key);
    if (it == store_.end()) return true;
    if (it->second.is_expired(now_ms)) {
        remove_key_locked(key);
        return true;
    }
    return false;
}

void StorageEngine::touch_lru_locked(const std::string& key) {
    auto it = lru_map_.find(key);
    if (it != lru_map_.end()) {
        lru_list_.erase(it->second);
    }
    lru_list_.push_front(key);
    lru_map_[key] = lru_list_.begin();
}

void StorageEngine::remove_key_locked(const std::string& key) {
    store_.erase(key);
    auto it = lru_map_.find(key);
    if (it != lru_map_.end()) {
        lru_list_.erase(it->second);
        lru_map_.erase(it);
    }
}

void StorageEngine::evict_if_needed_locked() {
    if (max_keys_ == 0 || store_.size() <= max_keys_) return;

    while (store_.size() > max_keys_ && !lru_list_.empty()) {
        std::string oldest_key = lru_list_.back();
        remove_key_locked(oldest_key);
    }
}

bool StorageEngine::set(const std::string& key, const std::string& value, int64_t expire_ms) {
    LockGuard lock(mutex_);
    int64_t expire_at = (expire_ms > 0) ? (current_time_ms() + expire_ms) : -1;
    store_[key] = Entry(value, expire_at);
    touch_lru_locked(key);
    evict_if_needed_locked();
    return true;
}

bool StorageEngine::get(const std::string& key, std::string& value) {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    if (is_expired_locked(key, now)) {
        return false;
    }
    auto it = store_.find(key);
    if (it == store_.end() || it->second.type != ValueType::STRING) {
        return false;
    }
    touch_lru_locked(key);
    value = it->second.string_val;
    return true;
}

bool StorageEngine::del(const std::string& key) {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    is_expired_locked(key, now);
    auto it = store_.find(key);
    if (it == store_.end()) return false;
    remove_key_locked(key);
    return true;
}

bool StorageEngine::exists(const std::string& key) {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    if (is_expired_locked(key, now)) return false;
    return store_.find(key) != store_.end();
}

bool StorageEngine::incrby(const std::string& key, int64_t delta, int64_t& new_value, std::string& err_msg) {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    is_expired_locked(key, now);

    auto it = store_.find(key);
    int64_t current_val = 0;
    int64_t existing_expire = -1;

    if (it != store_.end()) {
        if (it->second.type != ValueType::STRING) {
            err_msg = "WRONGTYPE Operation against a key holding the wrong kind of value";
            return false;
        }
        try {
            current_val = std::stoll(it->second.string_val);
        } catch (...) {
            err_msg = "ERR value is not an integer or out of range";
            return false;
        }
        existing_expire = it->second.expire_at_ms;
    }

    new_value = current_val + delta;
    store_[key] = Entry(std::to_string(new_value), existing_expire);
    touch_lru_locked(key);
    evict_if_needed_locked();
    return true;
}

bool StorageEngine::expire(const std::string& key, int64_t seconds) {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    if (is_expired_locked(key, now)) return false;

    auto it = store_.find(key);
    if (it == store_.end()) return false;

    if (seconds <= 0) {
        remove_key_locked(key);
    } else {
        it->second.expire_at_ms = now + (seconds * 1000);
    }
    return true;
}

int64_t StorageEngine::ttl(const std::string& key) {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    if (is_expired_locked(key, now)) return -2;

    auto it = store_.find(key);
    if (it == store_.end()) return -2;
    if (it->second.expire_at_ms == -1) return -1;

    int64_t remaining_ms = it->second.expire_at_ms - now;
    if (remaining_ms <= 0) {
        remove_key_locked(key);
        return -2;
    }
    return (remaining_ms + 999) / 1000;
}

bool StorageEngine::lpush(const std::string& key, const std::vector<std::string>& values, size_t& new_length) {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    is_expired_locked(key, now);

    auto it = store_.find(key);
    if (it != store_.end() && it->second.type != ValueType::LIST) {
        return false;
    }

    if (it == store_.end()) {
        store_[key] = Entry(std::vector<std::string>{});
        it = store_.find(key);
    }

    for (const auto& v : values) {
        it->second.list_val.insert(it->second.list_val.begin(), v);
    }
    touch_lru_locked(key);
    evict_if_needed_locked();
    new_length = it->second.list_val.size();
    return true;
}

bool StorageEngine::rpush(const std::string& key, const std::vector<std::string>& values, size_t& new_length) {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    is_expired_locked(key, now);

    auto it = store_.find(key);
    if (it != store_.end() && it->second.type != ValueType::LIST) {
        return false;
    }

    if (it == store_.end()) {
        store_[key] = Entry(std::vector<std::string>{});
        it = store_.find(key);
    }

    for (const auto& v : values) {
        it->second.list_val.push_back(v);
    }
    touch_lru_locked(key);
    evict_if_needed_locked();
    new_length = it->second.list_val.size();
    return true;
}

bool StorageEngine::lpop(const std::string& key, std::string& value) {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    if (is_expired_locked(key, now)) return false;

    auto it = store_.find(key);
    if (it == store_.end() || it->second.type != ValueType::LIST || it->second.list_val.empty()) {
        return false;
    }

    value = it->second.list_val.front();
    it->second.list_val.erase(it->second.list_val.begin());
    touch_lru_locked(key);
    return true;
}

bool StorageEngine::rpop(const std::string& key, std::string& value) {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    if (is_expired_locked(key, now)) return false;

    auto it = store_.find(key);
    if (it == store_.end() || it->second.type != ValueType::LIST || it->second.list_val.empty()) {
        return false;
    }

    value = it->second.list_val.back();
    it->second.list_val.pop_back();
    touch_lru_locked(key);
    return true;
}

bool StorageEngine::lrange(const std::string& key, int64_t start, int64_t stop, std::vector<std::string>& results) {
    LockGuard lock(mutex_);
    results.clear();
    int64_t now = current_time_ms();
    if (is_expired_locked(key, now)) return true;

    auto it = store_.find(key);
    if (it == store_.end()) return true;
    if (it->second.type != ValueType::LIST) return false;

    const auto& list = it->second.list_val;
    int64_t len = static_cast<int64_t>(list.size());
    if (len == 0) return true;

    if (start < 0) start = len + start;
    if (stop < 0) stop = len + stop;

    if (start < 0) start = 0;
    if (start >= len || start > stop) return true;
    if (stop >= len) stop = len - 1;

    for (int64_t i = start; i <= stop; ++i) {
        results.push_back(list[static_cast<size_t>(i)]);
    }
    touch_lru_locked(key);
    return true;
}

void StorageEngine::flushall() {
    LockGuard lock(mutex_);
    store_.clear();
    lru_list_.clear();
    lru_map_.clear();
}

size_t StorageEngine::dbsize() {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    size_t count = 0;
    for (auto it = store_.begin(); it != store_.end();) {
        if (it->second.is_expired(now)) {
            it = store_.erase(it);
        } else {
            ++count;
            ++it;
        }
    }
    return count;
}

std::vector<std::string> StorageEngine::get_all_keys() {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    std::vector<std::string> keys;
    for (const auto& pair : store_) {
        if (!pair.second.is_expired(now)) {
            keys.push_back(pair.first);
        }
    }
    return keys;
}

size_t StorageEngine::purge_expired() {
    LockGuard lock(mutex_);
    int64_t now = current_time_ms();
    size_t purged = 0;
    for (auto it = store_.begin(); it != store_.end();) {
        if (it->second.is_expired(now)) {
            auto lru_it = lru_map_.find(it->first);
            if (lru_it != lru_map_.end()) {
                lru_list_.erase(lru_it->second);
                lru_map_.erase(lru_it);
            }
            it = store_.erase(it);
            ++purged;
        } else {
            ++it;
        }
    }
    return purged;
}
