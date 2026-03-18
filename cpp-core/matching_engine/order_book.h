#pragma once

#include <vector>
#include <string>
#include <memory>
#include "../network/message_parser.h"

namespace trading {

class OrderBook {
public:
    using Symbol = std::string;
    using OrderPtr = std::shared_ptr<Order>;
    using Trades = std::vector<Trade>;

    explicit OrderBook(Symbol symbol);

    // Add order to the order book
    void add_order(OrderPtr order);
    
    // Cancellation and modification
    bool cancel_order(uint64_t order_id);
    bool modify_order(uint64_t order_id, uint32_t new_quantity, uint64_t new_price);
    
    // Order matching (returns list of trades)
    Trades match();

    // Getters for testing and inspection
    const std::vector<OrderPtr>& get_bids() const { return m_bids; }
    const std::vector<OrderPtr>& get_asks() const { return m_asks; }

private:
    using Orders = std::vector<OrderPtr>;
    Symbol m_symbol;
    Orders m_bids;
    Orders m_asks;
};

}