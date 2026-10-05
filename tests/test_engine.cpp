#include "../src/core/storage_engine.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/persistence/wal.hpp"
#include <iostream>
#include <cassert>
#include <windows.h>

void test_basic_crud() {
    std::cout << "[TEST] Running Basic CRUD Tests...\n";
    StorageEngine engine;
    std::string val;

    // SET & GET
    assert(engine.set("name", "Baadal"));
    assert(engine.get("name", val));
    assert(val == "Baadal");

    // EXISTS
    assert(engine.exists("name"));
    assert(!engine.exists("nonexistent"));

    // DEL
    assert(engine.del("name"));
    assert(!engine.get("name", val));
    assert(!engine.exists("name"));

    std::cout << "  -> Basic CRUD PASSED!\n";
}

void test_numeric_increment() {
    std::cout << "[TEST] Running Numeric Operations (INCR/DECR)...\n";
    StorageEngine engine;
    int64_t new_val = 0;
    std::string err;

    assert(engine.incrby("counter", 1, new_val, err));
    assert(new_val == 1);

    assert(engine.incrby("counter", 10, new_val, err));
    assert(new_val == 11);

    assert(engine.incrby("counter", -5, new_val, err));
    assert(new_val == 6);

    // Non-integer error check
    engine.set("str_val", "hello");
    assert(!engine.incrby("str_val", 1, new_val, err));
    assert(err.find("ERR") != std::string::npos);

    std::cout << "  -> Numeric Operations PASSED!\n";
}

void test_ttl_and_expiry() {
    std::cout << "[TEST] Running TTL & Expiration Tests...\n";
    StorageEngine engine;
    std::string val;

    // Set with 100ms expiry
    engine.set("temp_key", "temporary", 100);
    assert(engine.get("temp_key", val));
    assert(val == "temporary");

    // Wait 150ms
    Sleep(150);
    assert(!engine.get("temp_key", val)); // Should be expired and removed
    assert(!engine.exists("temp_key"));

    // EXPIRE command & TTL check
    engine.set("session", "user_abc");
    assert(engine.expire("session", 10));
    int64_t ttl_sec = engine.ttl("session");
    assert(ttl_sec > 0 && ttl_sec <= 10);

    std::cout << "  -> TTL & Expiration PASSED!\n";
}

void test_lru_eviction() {
    std::cout << "[TEST] Running LRU Eviction Tests (Max Keys = 3)...\n";
    StorageEngine engine(3); // Capacity = 3
    std::string val;

    engine.set("k1", "v1");
    engine.set("k2", "v2");
    engine.set("k3", "v3");

    // Access k1 so that k2 becomes the oldest unused key
    engine.get("k1", val);

    // Insert k4 -> should evict k2!
    engine.set("k4", "v4");

    assert(engine.exists("k1"));
    assert(!engine.exists("k2")); // k2 was evicted!
    assert(engine.exists("k3"));
    assert(engine.exists("k4"));

    std::cout << "  -> LRU Eviction PASSED!\n";
}

void test_list_operations() {
    std::cout << "[TEST] Running List Operations (LPUSH, RPUSH, LRANGE, POP)...\n";
    StorageEngine engine;
    size_t len = 0;
    std::string val;

    // RPUSH: a, b
    engine.rpush("mylist", {"a", "b"}, len);
    assert(len == 2);

    // LPUSH: z -> list is [z, a, b]
    engine.lpush("mylist", {"z"}, len);
    assert(len == 3);

    // LRANGE 0 -1 (get all)
    std::vector<std::string> items;
    assert(engine.lrange("mylist", 0, -1, items));
    assert(items.size() == 3);
    assert(items[0] == "z" && items[1] == "a" && items[2] == "b");

    // LPOP -> "z"
    assert(engine.lpop("mylist", val));
    assert(val == "z");

    // RPOP -> "b"
    assert(engine.rpop("mylist", val));
    assert(val == "b");

    std::cout << "  -> List Operations PASSED!\n";
}

void test_resp_parser() {
    std::cout << "[TEST] Running RESP Parser & Serialization Tests...\n";
    size_t consumed = 0;
    std::vector<std::string> tokens;

    // 1. Parse standard RESP Array: *3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
    std::string resp_cmd = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    assert(RespParser::parse_command(resp_cmd, consumed, tokens));
    assert(tokens.size() == 3);
    assert(tokens[0] == "SET" && tokens[1] == "foo" && tokens[2] == "bar");
    assert(consumed == resp_cmd.size());

    // 2. Parse inline command: "GET foo\r\n"
    std::string inline_cmd = "GET foo\r\n";
    assert(RespParser::parse_command(inline_cmd, consumed, tokens));
    assert(tokens.size() == 2);
    assert(tokens[0] == "GET" && tokens[1] == "foo");

    // 3. Serializers
    assert(RespParser::format_simple_string("OK") == "+OK\r\n");
    assert(RespParser::format_integer(42) == ":42\r\n");
    assert(RespParser::format_bulk_string("hi") == "$2\r\nhi\r\n");
    assert(RespParser::format_nil() == "$-1\r\n");

    std::cout << "  -> RESP Parser PASSED!\n";
}

void test_wal_recovery() {
    std::cout << "[TEST] Running WAL Persistence & Crash Recovery Tests...\n";
    std::string wal_path = "test_wal.aof";
    std::remove(wal_path.c_str());

    {
        StorageEngine engine;
        WalManager wal(wal_path);

        engine.set("persisted_key", "awesome_value");
        wal.append_command({"SET", "persisted_key", "awesome_value"});

        int64_t counter = 0;
        std::string err;
        engine.incrby("wal_counter", 100, counter, err);
        wal.append_command({"INCRBY", "wal_counter", "100"});

        size_t list_len = 0;
        engine.rpush("wal_list", {"item1", "item2"}, list_len);
        wal.append_command({"RPUSH", "wal_list", "item1", "item2"});

        int added = 0;
        engine.hset("wal_hash", "username", "baadal", added, err);
        engine.hset("wal_hash", "lang", "cpp", added, err);
        wal.append_command({"HSET", "wal_hash", "username", "baadal", "lang", "cpp"});
    }

    // Now simulate restart: Create brand new engine and recover from WAL
    {
        StorageEngine recovered_engine;
        WalManager recovery_wal(wal_path);
        size_t count = recovery_wal.recover(recovered_engine);
        assert(count == 4);

        std::string val;
        assert(recovered_engine.get("persisted_key", val));
        assert(val == "awesome_value");

        assert(recovered_engine.get("wal_counter", val));
        assert(val == "100");

        std::vector<std::string> list_items;
        assert(recovered_engine.lrange("wal_list", 0, -1, list_items));
        assert(list_items.size() == 2);
        assert(list_items[0] == "item1" && list_items[1] == "item2");

        std::string hash_val;
        bool hash_found = false;
        std::string hash_err;
        assert(recovered_engine.hget("wal_hash", "username", hash_val, hash_found, hash_err));
        assert(hash_found && hash_val == "baadal");
        assert(recovered_engine.hget("wal_hash", "lang", hash_val, hash_found, hash_err));
        assert(hash_found && hash_val == "cpp");
    }

    std::remove(wal_path.c_str());
    std::cout << "  -> WAL Persistence & Recovery PASSED!\n";
}

void test_hash_operations() {
    std::cout << "[TEST] Running Hash Operations (HSET, HGET, HDEL, HGETALL, etc.)...\n";
    StorageEngine engine;
    std::string err;
    int added = 0;

    // HSET new fields
    assert(engine.hset("user:100", "name", "Baadal", added, err));
    assert(added == 1);
    assert(engine.hset("user:100", "role", "Engineer", added, err));
    assert(added == 1);

    // HSET existing field (update)
    assert(engine.hset("user:100", "role", "Lead Engineer", added, err));
    assert(added == 0);

    // HGET
    std::string val;
    bool found = false;
    assert(engine.hget("user:100", "name", val, found, err));
    assert(found && val == "Baadal");
    assert(engine.hget("user:100", "role", val, found, err));
    assert(found && val == "Lead Engineer");
    assert(engine.hget("user:100", "nonexistent", val, found, err));
    assert(!found);

    // HEXISTS & HLEN
    bool exists = false;
    assert(engine.hexists("user:100", "name", exists, err));
    assert(exists);
    assert(engine.hexists("user:100", "unknown", exists, err));
    assert(!exists);

    size_t length = 0;
    assert(engine.hlen("user:100", length, err));
    assert(length == 2);

    // HGETALL
    std::vector<std::pair<std::string, std::string>> all_items;
    assert(engine.hgetall("user:100", all_items, err));
    assert(all_items.size() == 2);

    // HKEYS & HVALS
    std::vector<std::string> keys, vals;
    assert(engine.hkeys("user:100", keys, err));
    assert(keys.size() == 2);
    assert(engine.hvals("user:100", vals, err));
    assert(vals.size() == 2);

    // HDEL
    int deleted = 0;
    assert(engine.hdel("user:100", {"name"}, deleted, err));
    assert(deleted == 1);
    assert(engine.hlen("user:100", length, err));
    assert(length == 1);

    // Delete remaining field -> key should be deleted
    assert(engine.hdel("user:100", {"role"}, deleted, err));
    assert(deleted == 1);
    assert(!engine.exists("user:100"));

    // Type error test: Can't HSET on a string key
    engine.set("plain_str", "hello");
    assert(!engine.hset("plain_str", "f1", "v1", added, err));
    assert(err.find("WRONGTYPE") != std::string::npos);

    std::cout << "  -> Hash Operations PASSED!\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "  Running MiniRedis Comprehensive Tests   \n";
    std::cout << "==========================================\n";

    test_basic_crud();
    test_numeric_increment();
    test_ttl_and_expiry();
    test_lru_eviction();
    test_list_operations();
    test_hash_operations();
    test_resp_parser();
    test_wal_recovery();

    std::cout << "\n>>> ALL 8 TEST SUITES PASSED FLAWLESSLY! <<<\n";
    return 0;
}
