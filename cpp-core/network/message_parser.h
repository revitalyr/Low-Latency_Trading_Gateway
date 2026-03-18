#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <optional>

namespace trading {

// Message types
enum class MessageType : uint8_t {
    NEW_ORDER = 1,
    CANCEL_ORDER = 2,
    MODIFY_ORDER = 3,
    ORDER_ACK = 4,
    TRADE = 5
};

#pragma pack(push, 1)

struct MessageHeader {
    MessageType type;
    uint64_t length;
    uint64_t timestamp;
    uint32_t checksum;
};

struct NewOrder {
    uint64_t order_id;
    uint64_t price;
    uint32_t quantity;
    char symbol[8];
    char side; // 'B' for Buy, 'S' for Sell
};

struct CancelOrder {
    uint64_t order_id;
};

struct ModifyOrder {
    uint64_t order_id;
    uint64_t new_price;
    uint32_t new_quantity;
};

struct OrderAck {
    uint64_t order_id;
    bool accepted;
    uint64_t timestamp;
    char reason[64];
};

#pragma pack(pop)

enum class OrderCommand : uint8_t {
    ADD,
    CANCEL,
    MODIFY
};

struct Order {
    uint64_t id;
    uint64_t price;
    uint64_t quantity;
    char symbol[8];
    bool is_buy;
    uint64_t timestamp;
    OrderCommand command;

    Order() : command(OrderCommand::ADD) {}
    // Constructor for convenient creation
    Order(uint64_t id_, uint64_t price_, uint64_t qty_, const std::string& sym_, bool buy_)
        : id(id_), price(price_), quantity(qty_), is_buy(buy_), timestamp(0), command(OrderCommand::ADD) {
        std::memset(symbol, 0, sizeof(symbol));
        std::strncpy(symbol, sym_.c_str(), sizeof(symbol));
    }
};

struct Trade {
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    uint64_t price;
    uint64_t quantity;
    char symbol[8];
    uint64_t timestamp;

    Trade() = default;
    // Constructor for emplace_back
    Trade(uint64_t b_id, uint64_t s_id, uint64_t p, uint64_t q, const std::string& sym)
        : buy_order_id(b_id), sell_order_id(s_id), price(p), quantity(q), timestamp(0) {
        std::memset(symbol, 0, sizeof(symbol));
        std::strncpy(symbol, sym.c_str(), sizeof(symbol));
    }
};

class MessageParser {
public:
    static uint32_t calculate_checksum(const char* data, size_t length);
    static bool validate_message(const MessageHeader& header, const char* payload);
    
    static MessageHeader parse_header(const char* data, size_t length);
    static NewOrder parse_new_order(const char* data);
    static CancelOrder parse_cancel_order(const char* data);
    static ModifyOrder parse_modify_order(const char* data);

    static size_t serialize_message(MessageType type, const void* payload, char* buffer, size_t buffer_size);
    static size_t serialize_order_ack(const OrderAck& ack, char* buffer);
    static size_t serialize_trade(const Trade& trade, char* buffer);

    // Parse raw data into Order structure
    // In a real system would return std::variant<Order, Cancel, ...> or base Message class
    std::optional<Order> parse(const uint8_t* data, size_t len);
};

} // namespace trading