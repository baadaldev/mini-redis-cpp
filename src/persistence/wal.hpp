#ifndef WAL_HPP
#define WAL_HPP

#include "../core/storage_engine.hpp"
#include "../core/sync.hpp"
#include <string>
#include <vector>
#include <fstream>

class WalManager {
public:
    explicit WalManager(const std::string& filepath);
    ~WalManager();

    // Append a mutating command to the WAL file
    bool append_command(const std::vector<std::string>& tokens);

    // Replay WAL file into the storage engine on startup
    size_t recover(StorageEngine& engine);

    // Rewrite / compact the WAL from current memory state
    bool rewrite(StorageEngine& engine);

    void close();

private:
    std::string filepath_;
    std::ofstream log_file_;
    Mutex wal_mutex_;
};

#endif // WAL_HPP
