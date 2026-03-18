#pragma once

#include "../network/tcp_server.h"
#include "../network/message_parser.h"
#include "../matching_engine/matching_engine.h"
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>

namespace trading {

class GatewayServer {
private:
    std::unique_ptr<TCPServer> tcp_server_;
    std::unique_ptr<MatchingEngine> matching_engine_;
    std::atomic<bool> running_{false};
    
    // Client connections
    std::unordered_map<int, std::string> client_sessions_;
    
    // Message handling
    void handle_new_order(int client_fd, const NewOrder& order);
    void handle_cancel_order(int client_fd, const CancelOrder& order);
    void handle_modify_order(int client_fd, const ModifyOrder& order);
    
    // Response sending
    void send_order_ack(int client_fd, uint64_t order_id, bool accepted, const std::string& reason = "");
    void send_trade_notification(int client_fd, const ::trading::Trade& trade);
    
    // Session management
    std::string generate_session_id();
    void register_client(int client_fd);
    void unregister_client(int client_fd);
    
public:
    explicit GatewayServer(uint16_t port);
    ~GatewayServer();
    
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    // Disable copy and assignment
    GatewayServer(const GatewayServer&) = delete;
    GatewayServer& operator=(const GatewayServer&) = delete;
};

} // namespace trading
