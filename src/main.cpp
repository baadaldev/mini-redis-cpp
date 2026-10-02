#include "network/server.hpp"
#include <iostream>
#include <string>
#include <windows.h>

Server* g_server = nullptr;

BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        std::cout << "\n[INFO] Graceful shutdown initiated. Saving WAL...\n";
        if (g_server) {
            g_server->stop();
        }
        return TRUE;
    }
    return FALSE;
}

void print_banner() {
    std::cout << "\n"
              << "  __  __ _       _ _____          _ _       \n"
              << " |  \\/  (_)     (_)  __ \\        | (_)      \n"
              << " | \\  / |_ _ __  _| |__) |___  __| |_ ___   \n"
              << " | |\\/| | | '_ \\| |  _  // _ \\/ _` | / __|  \n"
              << " | |  | | | | | | | | \\ \\  __/ (_| | \\__ \\  \n"
              << " |_|  |_|_|_| |_|_|_|  \\_\\___|\\__,_|_|___/  \n"
              << "       High-Performance Key-Value Store      \n\n";
}

int main(int argc, char* argv[]) {
    int port = 6379;
    std::string aof_path = "mini_redis.aof";
    size_t max_keys = 0; // 0 = unlimited

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--aof" && i + 1 < argc) {
            aof_path = argv[++i];
        } else if (arg == "--max-keys" && i + 1 < argc) {
            max_keys = std::stoull(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: mini_redis.exe [--port <port>] [--aof <path>] [--max-keys <num>]\n";
            return 0;
        }
    }

    print_banner();
    SetConsoleCtrlHandler(console_handler, TRUE);

    Server server(port, aof_path, max_keys);
    g_server = &server;
    server.start();

    return 0;
}
