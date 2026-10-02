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
