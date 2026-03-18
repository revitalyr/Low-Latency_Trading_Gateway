#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <chrono>

namespace trading {

struct Order {
    uint64_t order_id;
    uint64_t quantity;
    uint64_t price;
    bool is_buy;
    std::string symbol;
    uint64_t timestamp;
    
    Order(uint64_t id, uint64_t qty, uint64_t px, bool buy, const std::string& sym)
        : order_id(id), quantity(qty), price(px), is_buy(buy), symbol(sym)
        , timestamp(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()) {}
};

enum class MessageType : uint8_t {
    NEW_ORDER = 1,
    CANCEL_ORDER = 2,
    MODIFY_ORDER = 3,
    ORDER_ACK = 4,
    ORDER_REJECT = 5,
    TRADE = 6,
    MARKET_DATA = 7
};

#pragma pack(push, 1)
struct MessageHeader {
    uint32_t length;      // Total message length including header
    uint32_t sequence;    // Sequence number
    uint64_t timestamp;   // Unix timestamp in nanoseconds
    MessageType type;     // Message type
    uint32_t checksum;    // Simple checksum for integrity
};

struct NewOrder {
    char symbol[16];      // Stock symbol (null-terminated)
    uint64_t order_id;    // Unique order identifier
    bool is_buy;          // true for buy, false for sell
    uint64_t quantity;    // Order quantity
    uint64_t price;       // Price in cents
    uint8_t order_type;   // 0=Market, 1=Limit
};

struct CancelOrder {
    uint64_t order_id;    // Order to cancel
};

struct ModifyOrder {
    uint64_t order_id;    // Order to modify
    uint64_t new_quantity; // New quantity
    uint64_t new_price;   // New price
};

struct OrderAck {
    uint64_t order_id;    // Order ID
    bool accepted;        // true if accepted
    uint64_t timestamp;   // Processing timestamp
    char reason[64];      // Rejection reason if not accepted
};

struct Trade {
    char symbol[16];      // Symbol
    uint64_t buy_order_id; // Buy order ID
    uint64_t sell_order_id; // Sell order ID
    uint64_t quantity;    // Trade quantity
    uint64_t price;       // Trade price
    uint64_t timestamp;   // Trade timestamp
};
#pragma pack(pop)

class MessageParser {
private:
    static uint32_t calculate_checksum(const char* data, size_t length);
    
public:
    static bool validate_message(const MessageHeader& header, const char* payload);
    static MessageHeader parse_header(const char* data, size_t length);
    static NewOrder parse_new_order(const char* data);
    static CancelOrder parse_cancel_order(const char* data);
    static ModifyOrder parse_modify_order(const char* data);
    
    static size_t serialize_message(MessageType type, const void* payload, 
                                   char* buffer, size_t buffer_size);
    static size_t serialize_order_ack(const OrderAck& ack, char* buffer);
    static size_t serialize_trade(const Trade& trade, char* buffer);
};

} // namespace trading
