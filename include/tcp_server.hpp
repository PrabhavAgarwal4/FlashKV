#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <sstream>
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include "storage_engine.hpp"
#include "wal.hpp"
#include "thread_pool.hpp"

namespace flashkv {

using std::string;
using std::stringstream;
using std::atomic;
using std::thread;
using std::size_t;

/**
 * @brief Helper struct initializing Winsock via RAII.
 */
struct WinsockContext {
    WinsockContext() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    }
    ~WinsockContext() {
        WSACleanup();
    }
};

/**
 * @brief Multi-client TCP Server connecting Network sockets to
 * StorageEngine and WriteAheadLog via a Worker ThreadPool.
 */
class TcpServer {
public:
    TcpServer(int port, StorageEngine& engine, WriteAheadLog& wal, size_t worker_threads = 4)
        : port_(port), engine_(engine), wal_(wal), pool_(worker_threads), running_(false) {}

    ~TcpServer() {
        stop();
    }

    // Non-copyable
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    /**
     * @brief Binds port, begins listening, and launches the accept loop thread.
     */
    bool start() {
        if (running_.load()) return true;

        listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_sock_ == INVALID_SOCKET) {
            std::cerr << "[ERROR] Failed to create socket: " << WSAGetLastError() << std::endl;
            return false;
        }

        // Enable address reuse
        int opt = 1;
        setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(static_cast<u_short>(port_));

        if (bind(listen_sock_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "[ERROR] Bind failed on port " << port_ << ": " << WSAGetLastError() << std::endl;
            closesocket(listen_sock_);
            listen_sock_ = INVALID_SOCKET;
            return false;
        }

        if (listen(listen_sock_, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "[ERROR] Listen failed: " << WSAGetLastError() << std::endl;
            closesocket(listen_sock_);
            listen_sock_ = INVALID_SOCKET;
            return false;
        }

        running_.store(true);
        accept_thread_ = thread(&TcpServer::accept_loop, this);
        return true;
    }

    /**
     * @brief Stops listening and gracefully terminates worker threads.
     */
    void stop() {
        if (!running_.load()) return;
        running_.store(false);

        if (listen_sock_ != INVALID_SOCKET) {
            closesocket(listen_sock_);
            listen_sock_ = INVALID_SOCKET;
        }

        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }

        pool_.shutdown();
    }

    bool is_running() const {
        return running_.load();
    }

    int port() const {
        return port_;
    }

private:
    void accept_loop() {
        while (running_.load()) {
            sockaddr_in client_addr{};
            int addr_len = sizeof(client_addr);

            SOCKET client_sock = accept(listen_sock_, (sockaddr*)&client_addr, &addr_len);
            if (client_sock == INVALID_SOCKET) {
                if (!running_.load()) break; // Normal shutdown
                continue;
            }

            // Dispatch handling of this client connection to the ThreadPool
            pool_.submit([this, client_sock] {
                handle_client(client_sock);
            });
        }
    }

    void handle_client(SOCKET client_sock) {
        char buffer[1024];
        string stream_accumulator;

        while (running_.load()) {
            int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                break; // Client disconnected or error
            }

            buffer[bytes_received] = '\0';
            stream_accumulator.append(buffer, bytes_received);

            // TCP Framing: Process all full lines ending in '\n'
            size_t newline_pos;
            while ((newline_pos = stream_accumulator.find('\n')) != string::npos) {
                string raw_line = stream_accumulator.substr(0, newline_pos);
                stream_accumulator.erase(0, newline_pos + 1);

                // Strip trailing '\r' if present
                if (!raw_line.empty() && raw_line.back() == '\r') {
                    raw_line.pop_back();
                }

                if (raw_line.empty()) continue;

                string response = execute_command(raw_line);
                send(client_sock, response.c_str(), static_cast<int>(response.length()), 0);

                if (raw_line == "QUIT") {
                    closesocket(client_sock);
                    return;
                }
            }
        }

        closesocket(client_sock);
    }

    string execute_command(const string& line) {
        stringstream ss(line);
        string cmd;
        ss >> cmd;

        // Convert command to uppercase
        for (auto& c : cmd) c = static_cast<char>(toupper(c));

        if (cmd == "PING") {
            return "+PONG\r\n";
        }

        if (cmd == "SET") {
            string key, value;
            if (ss >> key) {
                string rest;
                std::getline(ss, rest);
                if (!rest.empty() && rest.front() == ' ') {
                    rest.erase(0, 1);
                }
                value = rest;
                // WAL First (Durability), then in-memory storage
                wal_.append_set(key, value);
                engine_.set(key, value);
                return "+OK\r\n";
            }
            return "-ERR wrong number of arguments for 'set' command\r\n";
        }

        if (cmd == "GET") {
            string key;
            if (ss >> key) {
                auto val = engine_.get(key);
                if (val.has_value()) {
                    return "$" + *val + "\r\n";
                }
                return "(nil)\r\n";
            }
            return "-ERR wrong number of arguments for 'get' command\r\n";
        }

        if (cmd == "DEL") {
            string key;
            if (ss >> key) {
                wal_.append_del(key);
                bool removed = engine_.del(key);
                return removed ? ":1\r\n" : ":0\r\n";
            }
            return "-ERR wrong number of arguments for 'del' command\r\n";
        }

        if (cmd == "STATS") {
            return "+STATS keys:" + std::to_string(engine_.size()) +
                   " hits:" + std::to_string(engine_.hits()) +
                   " misses:" + std::to_string(engine_.misses()) +
                   " writes:" + std::to_string(engine_.writes()) + "\r\n";
        }

        if (cmd == "QUIT") {
            return "+BYE\r\n";
        }

        return "-ERR unknown command '" + cmd + "'\r\n";
    }

    WinsockContext ws_context_;
    int port_;
    SOCKET listen_sock_{INVALID_SOCKET};
    StorageEngine& engine_;
    WriteAheadLog& wal_;
    ThreadPool pool_;
    atomic<bool> running_;
    thread accept_thread_;
};

} // namespace flashkv
