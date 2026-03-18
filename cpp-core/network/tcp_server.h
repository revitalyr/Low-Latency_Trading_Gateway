#pragma once

#include <atomic>
#include <thread>
#include <vector>
#include <functional>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
#endif

namespace trading {

class TCPServer {
private:
    std::atomic<bool> running_{false};
    int server_fd_;
    uint16_t port_;
    std::vector<std::thread> worker_threads_;
    
    // Message handler callback
    std::function<void(int, const char*, size_t)> message_handler_;
    
    void accept_connections();
    void handle_client(int client_fd);
    
public:
    explicit TCPServer(uint16_t port);
    ~TCPServer();
    
    bool start();
    void stop();
    
    void set_message_handler(std::function<void(int, const char*, size_t)> handler);
    
    // Disable copy and assignment
    TCPServer(const TCPServer&) = delete;
    TCPServer& operator=(const TCPServer&) = delete;
};

} // namespace trading
