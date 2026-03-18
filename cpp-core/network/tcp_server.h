#pragma once

#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <cstdint>
#include <expected>
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif

namespace trading {

class TCPServer {
public:
    using Port = uint16_t;
    using StartResult = std::expected<void, std::string>;
    using SocketFd = int;
    using Buffer = const char*;
    using Length = size_t;
    using MessageHandler = std::function<void(SocketFd, Buffer, Length)>;
    using ConnectHandler = std::function<void(SocketFd)>;
    using DisconnectHandler = std::function<void(SocketFd)>;

    explicit TCPServer(Port port);
    ~TCPServer();

    [[nodiscard]] StartResult start();
    void stop();
    
    // Callback: client_fd, buffer, length
    void set_message_handler(MessageHandler handler);
    void set_connect_handler(ConnectHandler handler);
    void set_disconnect_handler(DisconnectHandler handler);

private:
    void accept_connections();
    void handle_client(SocketFd client_fd);

    using AtomicBool = std::atomic<bool>;
    using WorkerThreads = std::vector<std::thread>;

    Port m_port;
    SocketFd m_server_fd; // In implementation, SOCKET_ERROR_TYPE is masked, but int/SOCKET is used
    AtomicBool m_running{false};
    WorkerThreads m_worker_threads;
    MessageHandler m_message_handler;
    ConnectHandler m_connect_handler;
    DisconnectHandler m_disconnect_handler;
};

}