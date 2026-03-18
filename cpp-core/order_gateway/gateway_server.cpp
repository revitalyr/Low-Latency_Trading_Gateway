#include "gateway_server.h"
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define CLOSE_SOCKET closesocket
#else
    #include <unistd.h>
    #define CLOSE_SOCKET close
#endif

namespace trading {

GatewayServer::GatewayServer(uint16_t port) {
    tcp_server_ = std::make_unique<TCPServer>(port);
    matching_engine_ = std::make_unique<MatchingEngine>();
    
    // Set up message handler
    tcp_server_->set_message_handler([this](int client_fd, const char* data, size_t length) {
        // Parse message header
        if (length < sizeof(MessageHeader)) {
            return;
        }
        
        MessageHeader header = MessageParser::parse_header(data, length);
        
        // Validate message
        if (!MessageParser::validate_message(header, data + sizeof(MessageHeader))) {
            std::cerr << "Invalid message received" << std::endl;
            return;
        }
        
        // Handle different message types
        switch (header.type) {
            case MessageType::NEW_ORDER: {
                NewOrder order = MessageParser::parse_new_order(data + sizeof(MessageHeader));
                handle_new_order(client_fd, order);
                break;
            }
            case MessageType::CANCEL_ORDER: {
                CancelOrder order = MessageParser::parse_cancel_order(data + sizeof(MessageHeader));
                handle_cancel_order(client_fd, order);
                break;
            }
            case MessageType::MODIFY_ORDER: {
                ModifyOrder order = MessageParser::parse_modify_order(data + sizeof(MessageHeader));
                handle_modify_order(client_fd, order);
                break;
            }
            default:
                std::cerr << "Unknown message type: " << static_cast<int>(header.type) << std::endl;
                break;
        }
    });
}

GatewayServer::~GatewayServer() {
    stop();
}

bool GatewayServer::start() {
    if (!matching_engine_->start()) {
        std::cerr << "Failed to start matching engine" << std::endl;
        return false;
    }
    
    if (!tcp_server_->start()) {
        std::cerr << "Failed to start TCP server" << std::endl;
        matching_engine_->stop();
        return false;
    }
    
    running_.store(true);
    std::cout << "Gateway server started successfully" << std::endl;
    return true;
}

void GatewayServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (tcp_server_) {
        tcp_server_->stop();
    }
    
    if (matching_engine_) {
        matching_engine_->stop();
    }
    
    client_sessions_.clear();
    std::cout << "Gateway server stopped" << std::endl;
}

void GatewayServer::handle_new_order(int client_fd, const NewOrder& order) {
    std::string symbol(order.symbol);
    
    try {
        // Add order to matching engine
        auto trades = matching_engine_->add_order(order.order_id, symbol, order.is_buy, 
                                                  order.quantity, order.price);
        
        // Send acceptance
        send_order_ack(client_fd, order.order_id, true);
        
        // Send trade notifications
        for (const auto& trade : trades) {
            send_trade_notification(client_fd, trade);
        }
        
        std::cout << "New order processed: " << symbol << " " << order.quantity 
                  << " @ " << order.price << (order.is_buy ? " BUY" : " SELL") << std::endl;
        
    } catch (const std::exception& e) {
        send_order_ack(client_fd, order.order_id, false, e.what());
        std::cerr << "Error processing order: " << e.what() << std::endl;
    }
}

void GatewayServer::handle_cancel_order(int client_fd, const CancelOrder& order) {
    try {
        bool success = matching_engine_->cancel_order(order.order_id);
        send_order_ack(client_fd, order.order_id, success, 
                      success ? "" : "Order not found");
        
        std::cout << "Cancel order processed: " << order.order_id 
                  << (success ? " - Success" : " - Failed") << std::endl;
        
    } catch (const std::exception& e) {
        send_order_ack(client_fd, order.order_id, false, e.what());
        std::cerr << "Error cancelling order: " << e.what() << std::endl;
    }
}

void GatewayServer::handle_modify_order(int client_fd, const ModifyOrder& order) {
    try {
        bool success = matching_engine_->modify_order(order.order_id, order.new_quantity, order.new_price);
        send_order_ack(client_fd, order.order_id, success, 
                      success ? "" : "Order not found or invalid modification");
        
        std::cout << "Modify order processed: " << order.order_id 
                  << (success ? " - Success" : " - Failed") << std::endl;
        
    } catch (const std::exception& e) {
        send_order_ack(client_fd, order.order_id, false, e.what());
        std::cerr << "Error modifying order: " << e.what() << std::endl;
    }
}

void GatewayServer::send_order_ack(int client_fd, uint64_t order_id, bool accepted, const std::string& reason) {
    OrderAck ack{};
    ack.order_id = order_id;
    ack.accepted = accepted;
    ack.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    strncpy(ack.reason, reason.c_str(), sizeof(ack.reason) - 1);
    ack.reason[sizeof(ack.reason) - 1] = '\0';
    
    char buffer[sizeof(MessageHeader) + sizeof(OrderAck)];
    size_t message_size = MessageParser::serialize_order_ack(ack, buffer);
    
    if (message_size > 0) {
        send(client_fd, buffer, static_cast<int>(message_size), 0);
    }
}

void GatewayServer::send_trade_notification(int client_fd, const ::trading::Trade& trade) {
    char buffer[sizeof(MessageHeader) + sizeof(Trade)];
    size_t message_size = MessageParser::serialize_trade(trade, buffer);
    
    if (message_size > 0) {
        send(client_fd, buffer, static_cast<int>(message_size), 0);
    }
}

std::string GatewayServer::generate_session_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);
    
    std::stringstream ss;
    ss << "session_" << dis(gen);
    return ss.str();
}

void GatewayServer::register_client(int client_fd) {
    std::string session_id = generate_session_id();
    client_sessions_[client_fd] = session_id;
    
    std::cout << "Client registered: " << client_fd << " -> " << session_id << std::endl;
}

void GatewayServer::unregister_client(int client_fd) {
    auto it = client_sessions_.find(client_fd);
    if (it != client_sessions_.end()) {
        std::cout << "Client unregistered: " << it->second << std::endl;
        client_sessions_.erase(it);
    }
}

} // namespace trading
