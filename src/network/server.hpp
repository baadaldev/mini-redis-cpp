#ifndef SERVER_HPP
#define SERVER_HPP

#include "../core/storage_engine.hpp"
#include "../persistence/wal.hpp"
#include "../core/sync.hpp"
#include <winsock2.h>
#include <string>
#include <vector>

class Server {
public:
    Server(int port, const std::string& aof_path, size_t max_keys = 0);
    ~Server();

    // Start accepting client connections (blocking)
    void start();

    // Signal server to stop
    void stop();

private:
    int port_;
    std::string aof_path_;
    volatile bool running_;
    SOCKET listen_socket_;

    StorageEngine storage_;
    WalManager wal_;

    // Handle an individual client connection
    void handle_client(SOCKET client_sock);

    // Process a single command and return RESP formatted response
    std::string execute_command(const std::vector<std::string>& tokens);

    // Command handlers
    std::string handle_ping(const std::vector<std::string>& tokens);
    std::string handle_echo(const std::vector<std::string>& tokens);
    std::string handle_set(const std::vector<std::string>& tokens);
    std::string handle_get(const std::vector<std::string>& tokens);
    std::string handle_del(const std::vector<std::string>& tokens);
    std::string handle_exists(const std::vector<std::string>& tokens);
    std::string handle_expire(const std::vector<std::string>& tokens);
    std::string handle_ttl(const std::vector<std::string>& tokens);
    std::string handle_incr(const std::vector<std::string>& tokens, int64_t delta);
    std::string handle_lpush(const std::vector<std::string>& tokens);
    std::string handle_rpush(const std::vector<std::string>& tokens);
    std::string handle_lpop(const std::vector<std::string>& tokens);
    std::string handle_rpop(const std::vector<std::string>& tokens);
    std::string handle_lrange(const std::vector<std::string>& tokens);
    std::string handle_hset(const std::vector<std::string>& tokens);
    std::string handle_hget(const std::vector<std::string>& tokens);
    std::string handle_hdel(const std::vector<std::string>& tokens);
    std::string handle_hexists(const std::vector<std::string>& tokens);
    std::string handle_hlen(const std::vector<std::string>& tokens);
    std::string handle_hgetall(const std::vector<std::string>& tokens);
    std::string handle_hkeys(const std::vector<std::string>& tokens);
    std::string handle_hvals(const std::vector<std::string>& tokens);
    std::string handle_hmset(const std::vector<std::string>& tokens);
    std::string handle_hmget(const std::vector<std::string>& tokens);
    std::string handle_keys(const std::vector<std::string>& tokens);
    std::string handle_dbsize();
    std::string handle_flushall();
    std::string handle_info();
};

#endif // SERVER_HPP
