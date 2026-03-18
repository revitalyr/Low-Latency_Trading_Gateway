#include "tcp_server.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_TYPE SOCKET
    #define GET_LAST_ERROR WSAGetLastError()
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_TYPE int
    #define GET_LAST_ERROR errno
#endif

namespace trading {

TCPServer::TCPServer(uint16_t port) : port_(port), server_fd_(-1) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif
}

TCPServer::~TCPServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool TCPServer::start() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
        std::cerr << "Setsockopt failed" << std::endl;
        CLOSE_SOCKET(server_fd_);
        return false;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&address), 
             sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        CLOSE_SOCKET(server_fd_);
        return false;
    }

    if (listen(server_fd_, 128) < 0) {
        std::cerr << "Listen failed" << std::endl;
        CLOSE_SOCKET(server_fd_);
        return false;
    }

    running_.store(true);
    worker_threads_.emplace_back(&TCPServer::accept_connections, this);
    
    std::cout << "TCP Server started on port " << port_ << std::endl;
    return true;
}

void TCPServer::stop() {
    running_.store(false);
    
    if (server_fd_ >= 0) {
        CLOSE_SOCKET(server_fd_);
        server_fd_ = -1;
    }
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
}

void TCPServer::accept_connections() {
    while (running_.load()) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, 
                              reinterpret_cast<struct sockaddr*>(&client_addr), 
                              &client_len);
        
        if (client_fd < 0) {
            if (running_.load()) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }
        
        std::cout << "Client connected: " << inet_ntoa(client_addr.sin_addr) 
                  << ":" << ntohs(client_addr.sin_port) << std::endl;
        
        // Handle client in a separate thread
        worker_threads_.emplace_back([this, client_fd]() {
            handle_client(client_fd);
        });
    }
}

void TCPServer::handle_client(int client_fd) {
    char buffer[4096];
    
    while (running_.load()) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                std::cout << "Client disconnected" << std::endl;
            } else {
                std::cerr << "Recv error" << std::endl;
            }
            break;
        }
        
        if (message_handler_) {
            message_handler_(client_fd, buffer, static_cast<size_t>(bytes_read));
        }
    }
    
    CLOSE_SOCKET(client_fd);
}

void TCPServer::set_message_handler(std::function<void(int, const char*, size_t)> handler) {
    message_handler_ = std::move(handler);
}

} // namespace trading
