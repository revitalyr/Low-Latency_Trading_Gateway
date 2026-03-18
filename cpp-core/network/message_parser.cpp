#include "message_parser.h"
#include <chrono>
#include <iostream>

namespace trading {

uint32_t MessageParser::calculate_checksum(const char* data, size_t length) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < length; ++i) {
        checksum ^= static_cast<uint32_t>(data[i]) << (i % 4 * 8);
    }
    return checksum;
}

bool MessageParser::validate_message(const MessageHeader& header, const char* payload) {
    uint32_t expected_checksum = calculate_checksum(payload, header.length - sizeof(MessageHeader));
    return header.checksum == expected_checksum;
}

MessageHeader MessageParser::parse_header(const char* data, size_t length) {
    MessageHeader header{};
    if (length >= sizeof(MessageHeader)) {
        std::memcpy(&header, data, sizeof(MessageHeader));
    }
    return header;
}

NewOrder MessageParser::parse_new_order(const char* data) {
    NewOrder order{};
    std::memcpy(&order, data, sizeof(NewOrder));
    return order;
}

CancelOrder MessageParser::parse_cancel_order(const char* data) {
    CancelOrder order{};
    std::memcpy(&order, data, sizeof(CancelOrder));
    return order;
}

ModifyOrder MessageParser::parse_modify_order(const char* data) {
    ModifyOrder order{};
    std::memcpy(&order, data, sizeof(ModifyOrder));
    return order;
}

size_t MessageParser::serialize_message(MessageType type, const void* payload, 
                                       char* buffer, size_t buffer_size) {
    MessageHeader header{};
    header.type = type;
    header.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    size_t payload_size = 0;
    switch (type) {
        case MessageType::ORDER_ACK:
            payload_size = sizeof(OrderAck);
            break;
        case MessageType::TRADE:
            payload_size = sizeof(Trade);
            break;
        default:
            return 0;
    }
    
    header.length = sizeof(MessageHeader) + payload_size;
    
    if (buffer_size < header.length) {
        return 0;
    }
    
    // Copy header
    std::memcpy(buffer, &header, sizeof(MessageHeader));
    
    // Copy payload
    std::memcpy(buffer + sizeof(MessageHeader), payload, payload_size);
    
    // Calculate and set checksum
    header.checksum = calculate_checksum(buffer + sizeof(MessageHeader), payload_size);
    std::memcpy(buffer, &header, sizeof(MessageHeader));
    
    return header.length;
}

size_t MessageParser::serialize_order_ack(const OrderAck& ack, char* buffer) {
    return serialize_message(MessageType::ORDER_ACK, &ack, buffer, 
                           sizeof(MessageHeader) + sizeof(OrderAck));
}

size_t MessageParser::serialize_trade(const Trade& trade, char* buffer) {
    return serialize_message(MessageType::TRADE, &trade, buffer, 
                           sizeof(MessageHeader) + sizeof(Trade));
}

std::optional<Order> MessageParser::parse(const uint8_t* data, size_t len) {
    // Структура Wire-формата, совпадающая с тем, что отправляется в тестах
    #pragma pack(push, 1)
    struct WireNewOrder {
        uint8_t type;
        uint64_t order_id;
        uint64_t price;
        uint32_t quantity;
        char symbol[8];
        char side;
    };
    #pragma pack(pop)

    if (len < sizeof(WireNewOrder)) {
        return std::nullopt;
    }

    const auto* msg = reinterpret_cast<const WireNewOrder*>(data);

    if (static_cast<MessageType>(msg->type) != MessageType::NEW_ORDER) {
        return std::nullopt;
    }

    // Конвертация в Order (конструктор Order определён в хедере)
    std::string symbol_str(msg->symbol, strnlen(msg->symbol, 8));
    bool is_buy = (msg->side == 'B');
    return Order(msg->order_id, msg->price, msg->quantity, symbol_str, is_buy);
}

} // namespace trading
