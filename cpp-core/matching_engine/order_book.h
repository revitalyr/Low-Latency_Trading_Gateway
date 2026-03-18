#pragma once

#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <chrono>
#include "../network/message_parser.h"

namespace trading {

class OrderBook {
private:
    // Price level for orders
    struct PriceLevel {
        uint64_t price;
        uint64_t total_quantity;
        std::vector<std::shared_ptr<Order>> orders;
        
        explicit PriceLevel(uint64_t px) : price(px), total_quantity(0) {}
    };
    
    // Buy orders sorted by price descending (best bid first)
    std::vector<std::shared_ptr<PriceLevel>> buy_levels_;
    
    // Sell orders sorted by price ascending (best ask first)
    std::vector<std::shared_ptr<PriceLevel>> sell_levels_;
    
    // Order lookup by ID
    std::unordered_map<uint64_t, std::shared_ptr<Order>> orders_;
    
    mutable std::shared_mutex book_mutex_;
    
    std::string symbol_;
    
    // Helper methods
    std::shared_ptr<PriceLevel> find_or_create_price_level(uint64_t price, bool is_buy);
    void remove_price_level(std::shared_ptr<PriceLevel> level, bool is_buy);
    size_t find_price_level_index(uint64_t price, bool is_buy) const;
    
public:
    explicit OrderBook(const std::string& symbol);
    ~OrderBook() = default;
    
    // Core order book operations
    std::vector<Trade> add_order(std::shared_ptr<Order> order);
    bool cancel_order(uint64_t order_id);
    bool modify_order(uint64_t order_id, uint64_t new_quantity, uint64_t new_price);
    
    // Query methods
    uint64_t get_best_bid() const;
    uint64_t get_best_ask() const;
    uint64_t get_bid_size(uint64_t price) const;
    uint64_t get_ask_size(uint64_t price) const;
    
    // Market data
    std::string get_symbol() const { return symbol_; }
    size_t get_order_count() const;
    uint64_t get_total_volume() const;
    
    // Spread information
    uint64_t get_spread() const;
    
    // Disable copy and assignment
    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
};

} // namespace trading
