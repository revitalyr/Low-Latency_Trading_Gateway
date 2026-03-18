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
    #include <netinet/tcp.h>
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_TYPE int
    #define GET_LAST_ERROR errno
#endif

namespace trading {

TCPServer::TCPServer(Port port) : m_port(port), m_server_fd(-1) {
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

TCPServer::StartResult TCPServer::start() {
    m_server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_fd < 0) {
        // std::cerr << "Socket creation failed" << std::endl; // Logging moved to caller via error prop
        return std::unexpected("Socket creation failed");
    }

    int opt = 1;
    if (setsockopt(m_server_fd, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
        CLOSE_SOCKET(m_server_fd);
        return std::unexpected("Setsockopt failed");
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(m_port);

    if (bind(m_server_fd, reinterpret_cast<struct sockaddr*>(&address), 
             sizeof(address)) < 0) {
        CLOSE_SOCKET(m_server_fd);
        return std::unexpected("Bind failed on port " + std::to_string(m_port));
    }

    if (listen(m_server_fd, 128) < 0) {
        CLOSE_SOCKET(m_server_fd);
        return std::unexpected("Listen failed");
    }

    m_running.store(true);
    m_worker_threads.emplace_back(&TCPServer::accept_connections, this);
    
    std::cout << "TCP Server started on port " << m_port << std::endl;
    return {};
}

void TCPServer::stop() {
    m_running.store(false);
    
    if (m_server_fd >= 0) {
        CLOSE_SOCKET(m_server_fd);
        m_server_fd = -1;
    }
    
    for (auto& thread : m_worker_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_worker_threads.clear();
}

void TCPServer::accept_connections() {
    while (m_running.load()) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(m_server_fd, 
                              reinterpret_cast<struct sockaddr*>(&client_addr), 
                              &client_len);
        
        if (client_fd < 0) {
            if (m_running.load()) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }
        
        std::cout << "Client connected: " << inet_ntoa(client_addr.sin_addr) 
                  << ":" << ntohs(client_addr.sin_port) << std::endl;
        
        // Low-Latency Optimization: Disable Nagle's algorithm
        int flag = 1;
        if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, 
                       reinterpret_cast<const char*>(&flag), sizeof(flag)) < 0) {
            std::cerr << "Failed to set TCP_NODELAY" << std::endl;
        }

        // Handle client in a separate thread
        m_worker_threads.emplace_back([this, client_fd]() {
            handle_client(client_fd);
        });
    }
}

void TCPServer::handle_client(SocketFd client_fd) {
    if (m_connect_handler) {
        m_connect_handler(client_fd);
    }

    char buffer[4096];
    
    while (m_running.load()) {
#ifdef _WIN32
        int bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
#else
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
#endif
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                std::cout << "Client disconnected" << std::endl;
            } else {
                std::cerr << "Recv error" << std::endl;
            }
            break;
        }
        
        if (m_message_handler) {
            m_message_handler(client_fd, buffer, static_cast<size_t>(bytes_read));
        }
    }
    
    if (m_disconnect_handler) {
        m_disconnect_handler(client_fd);
    }

    CLOSE_SOCKET(client_fd);
}

void TCPServer::set_message_handler(MessageHandler handler) {
    m_message_handler = std::move(handler);
}

void TCPServer::set_connect_handler(ConnectHandler handler) {
    m_connect_handler = std::move(handler);
}

void TCPServer::set_disconnect_handler(DisconnectHandler handler) {
    m_disconnect_handler = std::move(handler);
}

} // namespace trading
