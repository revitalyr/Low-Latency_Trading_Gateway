#include <boost/ut.hpp>
#include <vector>
#include <cstring>
#include "message_parser.h"

using namespace boost::ut;

suite message_parser_tests = [] {
    "Parse valid NewOrder message"_test = [] {
        // Arrange: Создаем "сырой" буфер, имитирующий приход данных из сети.
        // Предполагаем структуру: [Type: 1B][OrderId: 8B][Price: 8B][Qty: 4B][Symbol: 8B][Side: 1B]
        // Адаптируйте этот блок под ваш реальный протокол в message_parser.h
        struct __attribute__((packed)) MockNewOrderMsg {
            uint8_t type;
            uint64_t order_id;
            uint64_t price;
            uint32_t quantity;
            char symbol[8];
            char side;
        };

        MockNewOrderMsg raw_msg{};
        raw_msg.type = 1; // MessageType::NewOrder (предположительно)
        raw_msg.order_id = 12345;
        raw_msg.price = 50000;
        raw_msg.quantity = 100;
        std::strncpy(raw_msg.symbol, "BTCUSD", 6);
        raw_msg.side = 'B'; // Buy

        std::vector<uint8_t> buffer(sizeof(MockNewOrderMsg));
        std::memcpy(buffer.data(), &raw_msg, sizeof(MockNewOrderMsg));

        // Act
        MessageParser parser;
        // Предполагаем сигнатуру parse(const uint8_t* data, size_t len) -> std::optional<Message>
        auto result = parser.parse(buffer.data(), buffer.size());

        // Assert
        expect(result.has_value()) << "Parser failed to parse a complete valid message";
        
        // Пример проверки полей (раскомментируйте и подправьте имена полей):
        // expect(result->type == MessageType::NewOrder);
        // expect(result->order_id == 12345_u);
    };

    "Handle incomplete message (fragmentation)"_test = [] {
        MessageParser parser;
        std::vector<uint8_t> partial_data = { 0x01, 0x00 }; // Только заголовок, нет тела

        auto result = parser.parse(partial_data.data(), partial_data.size());
        expect(not result.has_value()) << "Parser should not return a message for incomplete data";
    };
};