#pragma once

#include <cstdint>
#include <memory>
#include <atomic>
#include "../network/tcp_server.h"
#include <mutex>
#include <unordered_map>
#include "../matching_engine/matching_engine.h"
#include "../network/message_parser.h"
#include <expected>
#include <string>

namespace trading {

class GatewayServer {
public:
    using Port = uint16_t;
    using StartResult = std::expected<void, std::string>;
    explicit GatewayServer(Port port);
    ~GatewayServer();

    [[nodiscard]] StartResult start();
    void stop();
    bool is_running() const { return m_running; }

private:
    void handle_new_order(int client_fd, const NewOrder& order);
    void handle_cancel_order(int client_fd, const CancelOrder& order);
    void handle_modify_order(int client_fd, const ModifyOrder& order);

    void send_order_ack(int client_fd, uint64_t order_id, bool accepted, const std::string& reason);
    void send_trade_notification(int client_fd, const Trade& trade);

    std::string generate_session_id();
    void register_client(int client_fd);
    void unregister_client(int client_fd);

    using AtomicBool = std::atomic<bool>;
    using TcpServerPtr = std::unique_ptr<TCPServer>;
    using EnginePtr = std::unique_ptr<MatchingEngine>;
    using ClientSessions = std::unordered_map<int, std::string>;
    using OrderMap = std::unordered_map<uint64_t, int>; // Map order_id to client_fd
    using Mutex = std::mutex;

    Port m_port;
    AtomicBool m_running{false};
    
    TcpServerPtr m_tcp_server;
    EnginePtr m_matching_engine;
    ClientSessions m_client_sessions;
    OrderMap m_order_to_client;
    Mutex m_session_mutex;
};

}