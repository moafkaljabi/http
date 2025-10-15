/*

in .h :
    Creae class.
    declare constructor and destructor.

    create int m_socket = socket()
    create socket for our member function.
    wrap the socket in a function e.g. startServer.

in .cpp
    define the constructor and destructor.


in server.cpp: 
    make instance of the class TCPServer


*/      

// Logic/include/http_TCPServer.h
#pragma once

#include <string>
#include <atomic>

namespace http {

class TCPServer {
public:
    TCPServer(std::string ip, int port, int max_connections = 100);
    ~TCPServer();

    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;

    int startServer();            // create, bind, listen
    void startListen();           // accept loop (blocks)
    void stop();                  // request shutdown

private:
    std::string m_ip_address;
    int m_port;
    int m_listen_fd{-1};
    int m_max_connections;
    std::atomic<int> m_active_connections{0};
    std::atomic<bool> m_running{false};

    std::string m_server_message;

    int acceptConnection();
    void handleClient(int client_fd);
    std::string buildResponse(const std::string &body);
    bool sendAll(int fd, const char *buf, size_t len);
    void closeServer();
};

} // namespace http

