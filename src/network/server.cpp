#include "server.hpp"
#include "../protocol/resp_parser.hpp"
#include <iostream>
#include <algorithm>

Server::Server(int port, const std::string& aof_path, size_t max_keys)
    : port_(port), aof_path_(aof_path), running_(false), listen_socket_(INVALID_SOCKET),
      storage_(max_keys), wal_(aof_path) {}

Server::~Server() {
    stop();
}

void Server::stop() {
    running_ = false;
    if (listen_socket_ != INVALID_SOCKET) {
        closesocket(listen_socket_);
        listen_socket_ = INVALID_SOCKET;
    }
    wal_.close();
}

void Server::start() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[ERROR] WSAStartup failed.\n";
        return;
    }
#endif

    // Step 1: Recover data from WAL if file exists
    std::cout << "[INFO] Recovering state from WAL file: " << aof_path_ << "...\n";
    size_t recovered = wal_.recover(storage_);
    std::cout << "[INFO] Successfully replayed " << recovered << " commands from WAL. Current DB size: "
              << storage_.dbsize() << " keys.\n";

    // Step 2: Set up TCP Socket
    listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket_ == INVALID_SOCKET) {
        std::cerr << "[ERROR] Could not create socket\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    // Reuse address
    int opt = 1;
    setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(static_cast<u_short>(port_));

    if (bind(listen_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "[ERROR] Bind failed on port " << port_ << "\n";
        closesocket(listen_socket_);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[ERROR] Listen failed\n";
        closesocket(listen_socket_);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    running_ = true;
    std::cout << "\n=======================================================\n";
    std::cout << "  MiniRedis Server is running on port " << port_ << "\n";
    std::cout << "  Ready to accept connections (RESP & Redis-cli compatible)\n";
    std::cout << "=======================================================\n\n";

    // Background thread for active TTL expiration
    Thread::spawn_detached([this]() {
        while (running_) {
#ifdef _WIN32
            Sleep(1000);
#else
            usleep(1000 * 1000);
#endif
            if (!running_) break;
            storage_.purge_expired();
        }
    });

    // Main accept loop
    while (running_) {
        sockaddr_in client_addr;
#ifdef _WIN32
        int client_len = sizeof(client_addr);
#else
        socklen_t client_len = sizeof(client_addr);
#endif
        SOCKET client_sock = accept(listen_socket_, (sockaddr*)&client_addr, &client_len);

        if (client_sock == INVALID_SOCKET) {
            if (!running_) break;
            std::cerr << "[WARN] Accept error\n";
            continue;
        }

        // Handle each client in a detached worker thread
        Thread::spawn_detached([this, client_sock]() {
            handle_client(client_sock);
        });
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

void Server::handle_client(SOCKET client_sock) {
    char buffer[4096];
    std::string client_buffer;

    while (running_) {
        int bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            break; // Client disconnected or error
        }

        client_buffer.append(buffer, bytes_received);

        // Process all complete commands in buffer
        while (!client_buffer.empty()) {
            size_t bytes_consumed = 0;
            std::vector<std::string> tokens;

            if (!RespParser::parse_command(client_buffer, bytes_consumed, tokens) || bytes_consumed == 0) {
                // Incomplete command, wait for more data
                break;
            }

            client_buffer.erase(0, bytes_consumed);

            if (tokens.empty()) continue;

            // Check for QUIT
            std::string cmd = tokens[0];
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
            if (cmd == "QUIT") {
                std::string ok = RespParser::format_simple_string("OK");
                send(client_sock, ok.data(), static_cast<int>(ok.size()), 0);
                closesocket(client_sock);
                return;
            }

            std::string response = execute_command(tokens);
            send(client_sock, response.data(), static_cast<int>(response.size()), 0);
        }
    }

    closesocket(client_sock);
}

std::string Server::execute_command(const std::vector<std::string>& tokens) {
    if (tokens.empty()) return RespParser::format_error("no command");

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    if (cmd == "PING") return handle_ping(tokens);
    if (cmd == "ECHO") return handle_echo(tokens);
    if (cmd == "SET") return handle_set(tokens);
    if (cmd == "GET") return handle_get(tokens);
    if (cmd == "DEL") return handle_del(tokens);
    if (cmd == "EXISTS") return handle_exists(tokens);
    if (cmd == "EXPIRE") return handle_expire(tokens);
    if (cmd == "TTL") return handle_ttl(tokens);
    if (cmd == "INCR") return handle_incr(tokens, 1);
    if (cmd == "DECR") return handle_incr(tokens, -1);
    if (cmd == "INCRBY") {
        if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'incrby' command");
        try {
            int64_t delta = std::stoll(tokens[2]);
            return handle_incr(tokens, delta);
        } catch (...) {
            return RespParser::format_error("value is not an integer or out of range");
        }
    }
    if (cmd == "DECRBY") {
        if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'decrby' command");
        try {
            int64_t delta = std::stoll(tokens[2]);
            return handle_incr(tokens, -delta);
        } catch (...) {
            return RespParser::format_error("value is not an integer or out of range");
        }
    }
    if (cmd == "LPUSH") return handle_lpush(tokens);
    if (cmd == "RPUSH") return handle_rpush(tokens);
    if (cmd == "LPOP") return handle_lpop(tokens);
    if (cmd == "RPOP") return handle_rpop(tokens);
    if (cmd == "LRANGE") return handle_lrange(tokens);
    if (cmd == "HSET") return handle_hset(tokens);
    if (cmd == "HGET") return handle_hget(tokens);
    if (cmd == "HDEL") return handle_hdel(tokens);
    if (cmd == "HEXISTS") return handle_hexists(tokens);
    if (cmd == "HLEN") return handle_hlen(tokens);
    if (cmd == "HGETALL") return handle_hgetall(tokens);
    if (cmd == "HKEYS") return handle_hkeys(tokens);
    if (cmd == "HVALS") return handle_hvals(tokens);
    if (cmd == "HMSET") return handle_hmset(tokens);
    if (cmd == "HMGET") return handle_hmget(tokens);
    if (cmd == "SADD") return handle_sadd(tokens);
    if (cmd == "SMEMBERS") return handle_smembers(tokens);
    if (cmd == "SISMEMBER") return handle_sismember(tokens);
    if (cmd == "SREM") return handle_srem(tokens);
    if (cmd == "SCARD") return handle_scard(tokens);
    if (cmd == "TYPE") return handle_type(tokens);
    if (cmd == "MGET") return handle_mget(tokens);
    if (cmd == "MSET") return handle_mset(tokens);
    if (cmd == "KEYS") return handle_keys(tokens);
    if (cmd == "DBSIZE") return handle_dbsize();
    if (cmd == "FLUSHALL") return handle_flushall();
    if (cmd == "INFO") return handle_info();
    if (cmd == "BGREWRITEAOF") {
        wal_.rewrite(storage_);
        return RespParser::format_simple_string("Background append only file rewriting started");
    }

    return RespParser::format_error("unknown command '" + tokens[0] + "'");
}

std::string Server::handle_ping(const std::vector<std::string>& tokens) {
    if (tokens.size() == 1) {
        return RespParser::format_simple_string("PONG");
    }
    return RespParser::format_bulk_string(tokens[1]);
}

std::string Server::handle_echo(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'echo' command");
    return RespParser::format_bulk_string(tokens[1]);
}

std::string Server::handle_set(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'set' command");

    const std::string& key = tokens[1];
    const std::string& val = tokens[2];
    int64_t expire_ms = -1;

    for (size_t i = 3; i < tokens.size(); ++i) {
        std::string opt = tokens[i];
        std::transform(opt.begin(), opt.end(), opt.begin(), ::toupper);
        if (opt == "EX" && i + 1 < tokens.size()) {
            try {
                expire_ms = std::stoll(tokens[i + 1]) * 1000;
                i++;
            } catch (...) {
                return RespParser::format_error("invalid expire time in 'set' command");
            }
        } else if (opt == "PX" && i + 1 < tokens.size()) {
            try {
                expire_ms = std::stoll(tokens[i + 1]);
                i++;
            } catch (...) {
                return RespParser::format_error("invalid expire time in 'set' command");
            }
        }
    }

    storage_.set(key, val, expire_ms);
    wal_.append_command(tokens);
    return RespParser::format_simple_string("OK");
}

std::string Server::handle_get(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'get' command");
    std::string val;
    if (storage_.get(tokens[1], val)) {
        return RespParser::format_bulk_string(val);
    }
    return RespParser::format_nil();
}

std::string Server::handle_del(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'del' command");
    int64_t count = 0;
    for (size_t i = 1; i < tokens.size(); ++i) {
        if (storage_.del(tokens[i])) {
            count++;
        }
    }
    if (count > 0) {
        wal_.append_command(tokens);
    }
    return RespParser::format_integer(count);
}

std::string Server::handle_exists(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'exists' command");
    int64_t count = 0;
    for (size_t i = 1; i < tokens.size(); ++i) {
        if (storage_.exists(tokens[i])) {
            count++;
        }
    }
    return RespParser::format_integer(count);
}

std::string Server::handle_expire(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'expire' command");
    try {
        int64_t sec = std::stoll(tokens[2]);
        bool res = storage_.expire(tokens[1], sec);
        if (res) wal_.append_command(tokens);
        return RespParser::format_integer(res ? 1 : 0);
    } catch (...) {
        return RespParser::format_error("value is not an integer or out of range");
    }
}

std::string Server::handle_ttl(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'ttl' command");
    int64_t remaining = storage_.ttl(tokens[1]);
    return RespParser::format_integer(remaining);
}

std::string Server::handle_incr(const std::vector<std::string>& tokens, int64_t delta) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for increment command");
    int64_t new_val = 0;
    std::string err;
    if (storage_.incrby(tokens[1], delta, new_val, err)) {
        std::vector<std::string> log_cmd = {"INCRBY", tokens[1], std::to_string(delta)};
        wal_.append_command(log_cmd);
        return RespParser::format_integer(new_val);
    }
    return RespParser::format_error(err);
}

std::string Server::handle_lpush(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'lpush' command");
    std::vector<std::string> vals(tokens.begin() + 2, tokens.end());
    size_t new_len = 0;
    if (storage_.lpush(tokens[1], vals, new_len)) {
        wal_.append_command(tokens);
        return RespParser::format_integer(static_cast<int64_t>(new_len));
    }
    return RespParser::format_error("WRONGTYPE Operation against a key holding the wrong kind of value");
}

std::string Server::handle_rpush(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'rpush' command");
    std::vector<std::string> vals(tokens.begin() + 2, tokens.end());
    size_t new_len = 0;
    if (storage_.rpush(tokens[1], vals, new_len)) {
        wal_.append_command(tokens);
        return RespParser::format_integer(static_cast<int64_t>(new_len));
    }
    return RespParser::format_error("WRONGTYPE Operation against a key holding the wrong kind of value");
}

std::string Server::handle_lpop(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'lpop' command");
    std::string val;
    if (storage_.lpop(tokens[1], val)) {
        wal_.append_command(tokens);
        return RespParser::format_bulk_string(val);
    }
    return RespParser::format_nil();
}

std::string Server::handle_rpop(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'rpop' command");
    std::string val;
    if (storage_.rpop(tokens[1], val)) {
        wal_.append_command(tokens);
        return RespParser::format_bulk_string(val);
    }
    return RespParser::format_nil();
}

std::string Server::handle_lrange(const std::vector<std::string>& tokens) {
    if (tokens.size() < 4) return RespParser::format_error("wrong number of arguments for 'lrange' command");
    try {
        int64_t start = std::stoll(tokens[2]);
        int64_t stop = std::stoll(tokens[3]);
        std::vector<std::string> results;
        if (storage_.lrange(tokens[1], start, stop, results)) {
            return RespParser::format_array(results);
        }
        return RespParser::format_error("WRONGTYPE Operation against a key holding the wrong kind of value");
    } catch (...) {
        return RespParser::format_error("value is not an integer or out of range");
    }
}

std::string Server::handle_keys(const std::vector<std::string>& tokens) {
    std::vector<std::string> keys = storage_.get_all_keys();
    return RespParser::format_array(keys);
}

std::string Server::handle_dbsize() {
    return RespParser::format_integer(static_cast<int64_t>(storage_.dbsize()));
}

std::string Server::handle_flushall() {
    storage_.flushall();
    wal_.append_command({"FLUSHALL"});
    return RespParser::format_simple_string("OK");
}

std::string Server::handle_info() {
    std::string info = "# Server\r\nrole:master\r\nversion:1.0.0\r\ntcp_port:" + std::to_string(port_) +
                       "\r\nkeys:" + std::to_string(storage_.dbsize()) + "\r\n";
    return RespParser::format_bulk_string(info);
}

std::string Server::handle_hset(const std::vector<std::string>& tokens) {
    if (tokens.size() < 4 || (tokens.size() - 2) % 2 != 0) {
        return RespParser::format_error("wrong number of arguments for 'hset' command");
    }
    const std::string& key = tokens[1];
    int total_added = 0;
    std::string err;
    for (size_t i = 2; i + 1 < tokens.size(); i += 2) {
        int added = 0;
        if (!storage_.hset(key, tokens[i], tokens[i + 1], added, err)) {
            return RespParser::format_error(err);
        }
        total_added += added;
    }
    wal_.append_command(tokens);
    return RespParser::format_integer(total_added);
}

std::string Server::handle_hmset(const std::vector<std::string>& tokens) {
    if (tokens.size() < 4 || (tokens.size() - 2) % 2 != 0) {
        return RespParser::format_error("wrong number of arguments for 'hmset' command");
    }
    const std::string& key = tokens[1];
    std::string err;
    for (size_t i = 2; i + 1 < tokens.size(); i += 2) {
        int added = 0;
        if (!storage_.hset(key, tokens[i], tokens[i + 1], added, err)) {
            return RespParser::format_error(err);
        }
    }
    wal_.append_command(tokens);
    return RespParser::format_simple_string("OK");
}

std::string Server::handle_hget(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'hget' command");
    std::string value;
    bool found = false;
    std::string err;
    if (!storage_.hget(tokens[1], tokens[2], value, found, err)) {
        return RespParser::format_error(err);
    }
    if (found) return RespParser::format_bulk_string(value);
    return RespParser::format_nil();
}

std::string Server::handle_hmget(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'hmget' command");
    const std::string& key = tokens[1];
    std::vector<std::string> results;
    std::string err;
    for (size_t i = 2; i < tokens.size(); ++i) {
        std::string value;
        bool found = false;
        if (!storage_.hget(key, tokens[i], value, found, err)) {
            return RespParser::format_error(err);
        }
        if (found) {
            results.push_back(RespParser::format_bulk_string(value));
        } else {
            results.push_back(RespParser::format_nil());
        }
    }
    std::string resp = "*" + std::to_string(results.size()) + "\r\n";
    for (const auto& item : results) resp += item;
    return resp;
}

std::string Server::handle_hdel(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'hdel' command");
    std::vector<std::string> fields(tokens.begin() + 2, tokens.end());
    int deleted = 0;
    std::string err;
    if (!storage_.hdel(tokens[1], fields, deleted, err)) {
        return RespParser::format_error(err);
    }
    if (deleted > 0) wal_.append_command(tokens);
    return RespParser::format_integer(deleted);
}

std::string Server::handle_hexists(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'hexists' command");
    bool exists = false;
    std::string err;
    if (!storage_.hexists(tokens[1], tokens[2], exists, err)) {
        return RespParser::format_error(err);
    }
    return RespParser::format_integer(exists ? 1 : 0);
}

std::string Server::handle_hlen(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'hlen' command");
    size_t len = 0;
    std::string err;
    if (!storage_.hlen(tokens[1], len, err)) {
        return RespParser::format_error(err);
    }
    return RespParser::format_integer(static_cast<int64_t>(len));
}

std::string Server::handle_hgetall(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'hgetall' command");
    std::vector<std::pair<std::string, std::string>> items;
    std::string err;
    if (!storage_.hgetall(tokens[1], items, err)) {
        return RespParser::format_error(err);
    }
    std::vector<std::string> flat;
    for (const auto& p : items) {
        flat.push_back(p.first);
        flat.push_back(p.second);
    }
    return RespParser::format_array(flat);
}

std::string Server::handle_hkeys(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'hkeys' command");
    std::vector<std::string> keys;
    std::string err;
    if (!storage_.hkeys(tokens[1], keys, err)) {
        return RespParser::format_error(err);
    }
    return RespParser::format_array(keys);
}

std::string Server::handle_hvals(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'hvals' command");
    std::vector<std::string> vals;
    std::string err;
    if (!storage_.hvals(tokens[1], vals, err)) {
        return RespParser::format_error(err);
    }
    return RespParser::format_array(vals);
}

std::string Server::handle_sadd(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'sadd' command");
    const std::string& key = tokens[1];
    std::vector<std::string> members(tokens.begin() + 2, tokens.end());
    int added = 0;
    std::string err;
    if (!storage_.sadd(key, members, added, err)) {
        return RespParser::format_error(err);
    }
    if (added > 0) wal_.append_command(tokens);
    return RespParser::format_integer(added);
}

std::string Server::handle_smembers(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'smembers' command");
    const std::string& key = tokens[1];
    std::vector<std::string> members;
    std::string err;
    if (!storage_.smembers(key, members, err)) {
        return RespParser::format_error(err);
    }
    return RespParser::format_array(members);
}

std::string Server::handle_sismember(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'sismember' command");
    const std::string& key = tokens[1];
    const std::string& member = tokens[2];
    bool is_member = false;
    std::string err;
    if (!storage_.sismember(key, member, is_member, err)) {
        return RespParser::format_error(err);
    }
    return RespParser::format_integer(is_member ? 1 : 0);
}

std::string Server::handle_srem(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3) return RespParser::format_error("wrong number of arguments for 'srem' command");
    const std::string& key = tokens[1];
    std::vector<std::string> members(tokens.begin() + 2, tokens.end());
    int removed = 0;
    std::string err;
    if (!storage_.srem(key, members, removed, err)) {
        return RespParser::format_error(err);
    }
    if (removed > 0) wal_.append_command(tokens);
    return RespParser::format_integer(removed);
}

std::string Server::handle_scard(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'scard' command");
    const std::string& key = tokens[1];
    size_t card = 0;
    std::string err;
    if (!storage_.scard(key, card, err)) {
        return RespParser::format_error(err);
    }
    return RespParser::format_integer(static_cast<int64_t>(card));
}

std::string Server::handle_type(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'type' command");
    std::string t = storage_.type(tokens[1]);
    return RespParser::format_simple_string(t);
}

std::string Server::handle_mget(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return RespParser::format_error("wrong number of arguments for 'mget' command");
    std::vector<std::string> keys(tokens.begin() + 1, tokens.end());
    auto results = storage_.mget(keys);
    std::string resp = "*" + std::to_string(results.size()) + "\r\n";
    for (const auto& r : results) {
        if (r.first) {
            resp += RespParser::format_bulk_string(r.second);
        } else {
            resp += RespParser::format_nil();
        }
    }
    return resp;
}

std::string Server::handle_mset(const std::vector<std::string>& tokens) {
    if (tokens.size() < 3 || (tokens.size() - 1) % 2 != 0) {
        return RespParser::format_error("wrong number of arguments for 'mset' command");
    }
    std::vector<std::pair<std::string, std::string>> kvs;
    for (size_t i = 1; i + 1 < tokens.size(); i += 2) {
        kvs.push_back({tokens[i], tokens[i + 1]});
    }
    storage_.mset(kvs);
    wal_.append_command(tokens);
    return RespParser::format_simple_string("OK");
}


