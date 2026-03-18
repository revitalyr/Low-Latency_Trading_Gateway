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

GatewayServer::GatewayServer(Port port) : m_port(port) {
    m_tcp_server = std::make_unique<TCPServer>(port);
    m_matching_engine = std::make_unique<MatchingEngine>();
    
    // Set up session handlers
    m_tcp_server->set_connect_handler([this](int client_fd) {
        register_client(client_fd);
    });
    m_tcp_server->set_disconnect_handler([this](int client_fd) {
        unregister_client(client_fd);
    });

    // Set up message handler
    m_tcp_server->set_message_handler([this](int client_fd, const char* data, size_t length) {
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

    // Setup trade callback from matching engine
    m_matching_engine->set_trade_callback([this](const Trade& trade) {
        int buyer_fd = -1;
        int seller_fd = -1;

        // Look up FDs safely
        {
            std::lock_guard<Mutex> lock(m_session_mutex);
            if (m_order_to_client.contains(trade.buy_order_id)) {
                buyer_fd = m_order_to_client[trade.buy_order_id];
            }
            if (m_order_to_client.contains(trade.sell_order_id)) {
                seller_fd = m_order_to_client[trade.sell_order_id];
            }
        }

        // Notify buyer
        if (buyer_fd != -1) {
            send_trade_notification(buyer_fd, trade);
        }
        // Notify seller
        if (seller_fd != -1) {
            send_trade_notification(seller_fd, trade);
        }
    });
}

GatewayServer::~GatewayServer() {
    stop();
}

GatewayServer::StartResult GatewayServer::start() {
    // C++23 monadic operations could be used here (.and_then), but plain checks are readable too
    
    auto match_res = m_matching_engine->start();
    if (!match_res) {
        return std::unexpected("Failed to start matching engine: " + match_res.error());
    }
    
    auto tcp_res = m_tcp_server->start();
    if (!tcp_res) {
        m_matching_engine->stop();
        return std::unexpected("Failed to start TCP server: " + tcp_res.error());
    }
    
    m_running.store(true);
    std::cout << "Gateway server started successfully" << std::endl;
    return {};
}

void GatewayServer::stop() {
    if (!m_running.load()) {
        return;
    }
    
    m_running.store(false);
    
    if (m_tcp_server) {
        m_tcp_server->stop();
    }
    
    if (m_matching_engine) {
        m_matching_engine->stop();
    }
    
    m_client_sessions.clear();
    std::cout << "Gateway server stopped" << std::endl;
}

void GatewayServer::handle_new_order(int client_fd, const NewOrder& order) {
    std::string symbol(order.symbol);
    
    // Map order to client for future trade notifications
    {
        std::lock_guard<Mutex> lock(m_session_mutex);
        m_order_to_client[order.order_id] = client_fd;
    }
    
    try {
        // Add order to matching engine
        trading::Order core_order(order.order_id, order.price, order.quantity, symbol, order.side == 'B');
        m_matching_engine->add_order(core_order);
        
        // Send acceptance
        send_order_ack(client_fd, order.order_id, true, "");
        
        std::cout << "New order processed: " << symbol << " " << order.quantity 
                  << " @ " << order.price << (order.side == 'B' ? " BUY" : " SELL") << std::endl;
        
    } catch (const std::exception& e) {
        send_order_ack(client_fd, order.order_id, false, e.what());
        std::cerr << "Error processing order: " << e.what() << std::endl;
    }
}

void GatewayServer::handle_cancel_order(int client_fd, const CancelOrder& order) {
    try {
        bool success = m_matching_engine->cancel_order(order.order_id);
        send_order_ack(client_fd, order.order_id, success, 
                      success ? "" : "Cancel request rejected (queue full)");
        
        std::cout << "Cancel order processed: " << order.order_id 
                  << (success ? " - Success" : " - Failed") << std::endl;
        
    } catch (const std::exception& e) {
        send_order_ack(client_fd, order.order_id, false, e.what());
        std::cerr << "Error cancelling order: " << e.what() << std::endl;
    }
}

void GatewayServer::handle_modify_order(int client_fd, const ModifyOrder& order) {
    try {
        bool success = m_matching_engine->modify_order(order.order_id, order.new_quantity, order.new_price);
        send_order_ack(client_fd, order.order_id, success, 
                      success ? "" : "Modify request rejected (queue full)");
        
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
    {
        std::lock_guard<Mutex> lock(m_session_mutex);
        m_client_sessions[client_fd] = session_id;
    }
    std::cout << "Client registered: " << client_fd << " -> " << session_id << std::endl;
}

void GatewayServer::unregister_client(int client_fd) {
    std::string session_id;
    {
        std::lock_guard<Mutex> lock(m_session_mutex);
        auto it = m_client_sessions.find(client_fd);
        if (it != m_client_sessions.end()) {
            session_id = it->second;
            m_client_sessions.erase(it);
        }
        
        // Clean up associated orders mapping
        // Note: In high-load systems, we might use a separate index or lazy cleanup
        std::erase_if(m_order_to_client, [client_fd](const auto& item) {
            return item.second == client_fd;
        });
    }
    
    if (!session_id.empty()) {
        std::cout << "Client unregistered: " << session_id << std::endl;
    }
}

} // namespace trading
