#pragma once

#include "order_book.h"
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include "../lockfree_queue/lockfree_queue.h"

namespace trading {

class MatchingEngine {
private:
    // Order books by symbol
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books_;
    
    // Thread safety
    mutable std::shared_mutex engine_mutex_;
    
    // Message queue for processing orders
    LockFreeQueue<std::shared_ptr<Order>, 10000> order_queue_;
    
    // Worker threads
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_{false};
    
    // Statistics
    std::atomic<uint64_t> orders_processed_{0};
    std::atomic<uint64_t> trades_executed_{0};
    std::atomic<uint64_t> total_volume_{0};
    
    // Worker thread function
    void process_orders();
    
    // Internal methods
    std::shared_ptr<OrderBook> get_or_create_order_book(const std::string& symbol);
    void handle_trade(const Trade& trade);
    
public:
    MatchingEngine();
    ~MatchingEngine();
    
    // Engine control
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    // Order operations
    std::vector<Trade> add_order(uint64_t order_id, const std::string& symbol, 
                                bool is_buy, uint64_t quantity, uint64_t price);
    bool cancel_order(uint64_t order_id);
    bool modify_order(uint64_t order_id, uint64_t new_quantity, uint64_t new_price);
    
    // Query operations
    uint64_t get_best_bid(const std::string& symbol) const;
    uint64_t get_best_ask(const std::string& symbol) const;
    uint64_t get_spread(const std::string& symbol) const;
    
    // Statistics
    uint64_t get_orders_processed() const { return orders_processed_.load(); }
    uint64_t get_trades_executed() const { return trades_executed_.load(); }
    uint64_t get_total_volume() const { return total_volume_.load(); }
    
    // Market data
    std::vector<std::string> get_symbols() const;
    size_t get_order_count(const std::string& symbol) const;
    
    // Disable copy and assignment
    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;
};

} // namespace trading
