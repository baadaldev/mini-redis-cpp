#include "wal.hpp"
#include "../protocol/resp_parser.hpp"
#include <iostream>
#include <cstdio>
#include <algorithm>

WalManager::WalManager(const std::string& filepath) : filepath_(filepath) {
    log_file_.open(filepath_.c_str(), std::ios::out | std::ios::app | std::ios::binary);
}

WalManager::~WalManager() {
    close();
}

void WalManager::close() {
    LockGuard lock(wal_mutex_);
    if (log_file_.is_open()) {
        log_file_.flush();
        log_file_.close();
    }
}

bool WalManager::append_command(const std::vector<std::string>& tokens) {
    if (tokens.empty()) return true;

    std::string serialized = RespParser::format_array(tokens);

    LockGuard lock(wal_mutex_);
    if (!log_file_.is_open()) {
        log_file_.open(filepath_.c_str(), std::ios::out | std::ios::app | std::ios::binary);
    }

    if (log_file_.is_open()) {
        log_file_.write(serialized.data(), serialized.size());
        log_file_.flush(); // Ensure durability on disk
        return true;
    }
    return false;
}

size_t WalManager::recover(StorageEngine& engine) {
    std::ifstream in(filepath_.c_str(), std::ios::in | std::ios::binary);
    if (!in.is_open()) return 0;

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();

    size_t offset = 0;
    size_t replayed_count = 0;

    while (offset < content.size()) {
        std::string buffer = content.substr(offset);
        size_t consumed = 0;
        std::vector<std::string> tokens;

        if (!RespParser::parse_command(buffer, consumed, tokens) || consumed == 0) {
            break;
        }

        offset += consumed;

        if (tokens.empty()) continue;

        std::string cmd = tokens[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

        if (cmd == "SET" && tokens.size() >= 3) {
            engine.set(tokens[1], tokens[2]);
            replayed_count++;
        } else if (cmd == "DEL" && tokens.size() >= 2) {
            engine.del(tokens[1]);
            replayed_count++;
        } else if (cmd == "INCRBY" && tokens.size() >= 3) {
            int64_t val = 0;
            std::string err;
            engine.incrby(tokens[1], std::stoll(tokens[2]), val, err);
            replayed_count++;
        } else if (cmd == "EXPIRE" && tokens.size() >= 3) {
            engine.expire(tokens[1], std::stoll(tokens[2]));
            replayed_count++;
        } else if (cmd == "RPUSH" && tokens.size() >= 3) {
            std::vector<std::string> vals(tokens.begin() + 2, tokens.end());
            size_t new_len = 0;
            engine.rpush(tokens[1], vals, new_len);
            replayed_count++;
        } else if (cmd == "LPUSH" && tokens.size() >= 3) {
            std::vector<std::string> vals(tokens.begin() + 2, tokens.end());
            size_t new_len = 0;
            engine.lpush(tokens[1], vals, new_len);
            replayed_count++;
        } else if (cmd == "HSET" && tokens.size() >= 4) {
            for (size_t i = 2; i + 1 < tokens.size(); i += 2) {
                int added = 0;
                std::string err;
                engine.hset(tokens[1], tokens[i], tokens[i + 1], added, err);
            }
            replayed_count++;
        } else if (cmd == "HDEL" && tokens.size() >= 3) {
            std::vector<std::string> fields(tokens.begin() + 2, tokens.end());
            int deleted = 0;
            std::string err;
            engine.hdel(tokens[1], fields, deleted, err);
            replayed_count++;
        } else if (cmd == "SADD" && tokens.size() >= 3) {
            std::vector<std::string> members(tokens.begin() + 2, tokens.end());
            int added = 0;
            std::string err;
            engine.sadd(tokens[1], members, added, err);
            replayed_count++;
        } else if (cmd == "SREM" && tokens.size() >= 3) {
            std::vector<std::string> members(tokens.begin() + 2, tokens.end());
            int removed = 0;
            std::string err;
            engine.srem(tokens[1], members, removed, err);
            replayed_count++;
        } else if (cmd == "MSET" && tokens.size() >= 3) {
            std::vector<std::pair<std::string, std::string>> kvs;
            for (size_t i = 1; i + 1 < tokens.size(); i += 2) {
                kvs.push_back({tokens[i], tokens[i + 1]});
            }
            engine.mset(kvs);
            replayed_count++;
        } else if (cmd == "FLUSHALL") {
            engine.flushall();
            replayed_count++;
        }
    }

    return replayed_count;
}

bool WalManager::rewrite(StorageEngine& engine) {
    std::string tmp_path = filepath_ + ".tmp";
    std::ofstream tmp_file(tmp_path.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!tmp_file.is_open()) return false;

    std::vector<std::string> keys = engine.get_all_keys();
    for (const auto& key : keys) {
        std::string val;
        std::vector<std::string> list;
        std::unordered_map<std::string, std::string> hash_map;
        std::unordered_set<std::string> set_members;

        if (engine.get(key, val)) {
            int64_t remaining_ttl = engine.ttl(key);
            std::vector<std::string> set_cmd = {"SET", key, val};
            std::string serialized = RespParser::format_array(set_cmd);
            tmp_file.write(serialized.data(), serialized.size());

            if (remaining_ttl > 0) {
                std::vector<std::string> expire_cmd = {"EXPIRE", key, std::to_string(remaining_ttl)};
                std::string exp_ser = RespParser::format_array(expire_cmd);
                tmp_file.write(exp_ser.data(), exp_ser.size());
            }
        } else if (engine.get_list(key, list) && !list.empty()) {
            std::vector<std::string> rpush_cmd = {"RPUSH", key};
            rpush_cmd.insert(rpush_cmd.end(), list.begin(), list.end());
            std::string serialized = RespParser::format_array(rpush_cmd);
            tmp_file.write(serialized.data(), serialized.size());
        } else if (engine.get_hash(key, hash_map) && !hash_map.empty()) {
            std::vector<std::string> hset_cmd = {"HSET", key};
            for (const auto& pair : hash_map) {
                hset_cmd.push_back(pair.first);
                hset_cmd.push_back(pair.second);
            }
            std::string serialized = RespParser::format_array(hset_cmd);
            tmp_file.write(serialized.data(), serialized.size());
        } else if (engine.get_set(key, set_members) && !set_members.empty()) {
            std::vector<std::string> sadd_cmd = {"SADD", key};
            sadd_cmd.insert(sadd_cmd.end(), set_members.begin(), set_members.end());
            std::string serialized = RespParser::format_array(sadd_cmd);
            tmp_file.write(serialized.data(), serialized.size());
        }
    }

    tmp_file.flush();
    tmp_file.close();

    close();
    std::remove(filepath_.c_str());
    std::rename(tmp_path.c_str(), filepath_.c_str());

    log_file_.open(filepath_.c_str(), std::ios::out | std::ios::app | std::ios::binary);
    return true;
}
