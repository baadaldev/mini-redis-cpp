#ifndef ENTRY_HPP
#define ENTRY_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <cstdint>

enum class ValueType {
    STRING,
    LIST,
    HASH,
    SET
};

struct Entry {
    ValueType type;
    std::string string_val;
    std::vector<std::string> list_val;
    std::unordered_map<std::string, std::string> hash_val;
    std::unordered_set<std::string> set_val;
    int64_t expire_at_ms; // -1 means no expiration, otherwise epoch timestamp in ms

    Entry() : type(ValueType::STRING), expire_at_ms(-1) {}
    
    explicit Entry(const std::string& str, int64_t expire_ms = -1)
        : type(ValueType::STRING), string_val(str), expire_at_ms(expire_ms) {}

    explicit Entry(const std::vector<std::string>& list, int64_t expire_ms = -1)
        : type(ValueType::LIST), list_val(list), expire_at_ms(expire_ms) {}

    explicit Entry(const std::unordered_map<std::string, std::string>& h, int64_t expire_ms = -1)
        : type(ValueType::HASH), hash_val(h), expire_at_ms(expire_ms) {}

    explicit Entry(const std::unordered_set<std::string>& s, int64_t expire_ms = -1)
        : type(ValueType::SET), set_val(s), expire_at_ms(expire_ms) {}

    bool is_expired(int64_t now_ms) const {
        if (expire_at_ms == -1) return false;
        return now_ms >= expire_at_ms;
    }
};

#endif // ENTRY_HPP

