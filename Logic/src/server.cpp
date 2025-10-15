// Logic/src/server.cpp
#include "../include/http_TCPServer.h"

#include <csignal>
#include <iostream>
#include <memory>

static std::atomic<bool> g_stop_requested{false};
static http::TCPServer *g_server_ptr = nullptr;

extern "C" void handle_sigint(int) {
    g_stop_requested.store(true);
    if (g_server_ptr) {
        g_server_ptr->stop();
    }
}

int main() {
    std::signal(SIGINT, handle_sigint);
    std::signal(SIGTERM, handle_sigint);

    http::TCPServer server("0.0.0.0", 8080, 200);
    g_server_ptr = &server;

    if (server.startServer() != 0) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }

    server.startListen();

    std::cout << "Shutting down." << std::endl;
    return 0;
}
