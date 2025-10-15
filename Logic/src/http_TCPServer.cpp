// Logic/src/http_TCPServer.cpp
#include "../include/http_TCPServer.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <sstream>
#include <atomic>
#include <algorithm>

namespace {

void log(const std::string &msg) {
    std::cerr << msg << std::endl;
}

} 

namespace http {

TCPServer::TCPServer(std::string ip, int port, int max_connections)
    : m_ip_address(std::move(ip)),
      m_port(port),
      m_max_connections(max_connections),
      m_server_message(buildResponse(R"(<html><body><h1>OK</h1></body></html>)"))
{
}

TCPServer::~TCPServer() {
    stop();
    closeServer();
}

int TCPServer::startServer() {
    m_listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_fd < 0) {
        log("socket() failed: " + std::string(std::strerror(errno)));
        return -1;
    }

    int opt = 1;
    if (setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log("setsockopt(SO_REUSEADDR) failed: " + std::string(std::strerror(errno)));
        ::close(m_listen_fd);
        m_listen_fd = -1;
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(m_port));
    if (m_ip_address.empty() || m_ip_address == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, m_ip_address.c_str(), &addr.sin_addr) != 1) {
            log("inet_pton failed for address: " + m_ip_address);
            ::close(m_listen_fd);
            m_listen_fd = -1;
            return -1;
        }
    }

    if (bind(m_listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        log("bind() failed: " + std::string(std::strerror(errno)));
        ::close(m_listen_fd);
        m_listen_fd = -1;
        return -1;
    }

    if (listen(m_listen_fd, 128) < 0) {
        log("listen() failed: " + std::string(std::strerror(errno)));
        ::close(m_listen_fd);
        m_listen_fd = -1;
        return -1;
    }

    // make the listening socket non-blocking for select/timeouts
    int flags = fcntl(m_listen_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(m_listen_fd, F_SETFL, flags | O_NONBLOCK);
    }

    m_running.store(true);
    log("Server started on " + m_ip_address + ":" + std::to_string(m_port));
    return 0;
}

void TCPServer::startListen() {
    if (m_listen_fd < 0) {
        log("Server not started: call startServer() first");
        return;
    }

    while (m_running.load()) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_listen_fd, &readfds);

        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int nfds = m_listen_fd + 1;
        int sel = select(nfds, &readfds, nullptr, nullptr, &tv);
        if (sel < 0) {
            if (errno == EINTR) continue;
            log("select() error: " + std::string(std::strerror(errno)));
            break;
        } else if (sel == 0) {
            continue; // timeout, loop again and check running flag
        }

        if (FD_ISSET(m_listen_fd, &readfds)) {
            if (m_active_connections.load() >= m_max_connections) {
                // accept and immediately close to refuse connection politely
                int client_fd = accept(m_listen_fd, nullptr, nullptr);
                if (client_fd >= 0) {
                    log("Max connections reached, refusing new connection");
                    ::shutdown(client_fd, SHUT_RDWR);
                    ::close(client_fd);
                }
                continue;
            }

            int client_fd = accept(m_listen_fd, nullptr, nullptr);
            if (client_fd < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) continue;
                log("accept() failed: " + std::string(std::strerror(errno)));
                continue;
            }

            // increment active count and detach a worker thread
            m_active_connections.fetch_add(1);
            std::thread([this, client_fd]() {
                handleClient(client_fd);
                m_active_connections.fetch_sub(1);
            }).detach();
        }
    }

    // Wait for active connections to drain (bounded short wait)
    const int max_wait_seconds = 5;
    for (int i = 0; i < max_wait_seconds && m_active_connections.load() > 0; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    log("Server stopped listening");
}

void TCPServer::stop() {
    if (!m_running.exchange(false)) return;
    if (m_listen_fd >= 0) {
        ::shutdown(m_listen_fd, SHUT_RDWR);
        ::close(m_listen_fd);
        m_listen_fd = -1;
    }
}

int TCPServer::acceptConnection() {
    if (m_listen_fd < 0) return -1;
    int client_fd = accept(m_listen_fd, nullptr, nullptr);
    return client_fd;
}

void TCPServer::handleClient(int client_fd) {
    if (client_fd < 0) return;
    constexpr size_t BUFFER_SIZE = 32768;
    std::vector<char> buffer(BUFFER_SIZE);
    ssize_t n = recv(client_fd, buffer.data(), static_cast<int>(buffer.size()) - 1, 0);
    if (n <= 0) {
        ::close(client_fd);
        return;
    }

    // Simple request parsing: ignore details, just log first line
    buffer[std::max<ssize_t>(0, n-1)] = '\0';
    std::string request(buffer.data(), static_cast<size_t>(n));
    {
        std::istringstream iss(request);
        std::string request_line;
        if (std::getline(iss, request_line)) {
            // trim CR
            if (!request_line.empty() && request_line.back() == '\r') request_line.pop_back();
            log("REQ: " + request_line);
        }
    }

    // send prepared response
    const std::string &resp = m_server_message;
    if (!sendAll(client_fd, resp.data(), resp.size())) {
        log("Failed to send response to client");
    }

    ::shutdown(client_fd, SHUT_RDWR);
    ::close(client_fd);
}

std::string TCPServer::buildResponse(const std::string &body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: text/html; charset=utf-8\r\n";
    oss << "Content-Length: " << body.size() << "\r\n";
    oss << "Connection: close\r\n";
    oss << "\r\n";
    oss << body;
    return oss.str();
}

bool TCPServer::sendAll(int fd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd, buf + sent, static_cast<int>(len - sent), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

void TCPServer::closeServer() {
    if (m_listen_fd >= 0) {
        ::shutdown(m_listen_fd, SHUT_RDWR);
        ::close(m_listen_fd);
        m_listen_fd = -1;
    }
}

} // namespace http
