#include "matching_engine.h"
#include <iostream>

namespace trading {

MatchingEngine::MatchingEngine() {
}

MatchingEngine::~MatchingEngine() {
    stop();
}

bool MatchingEngine::start() {
    if (running_.load()) {
        return false;
    }
    
    running_.store(true);
    
    // Start worker threads
    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) {
        num_threads = 4; // Default to 4 threads
    }
    
    for (size_t i = 0; i < num_threads; ++i) {
        worker_threads_.emplace_back(&MatchingEngine::process_orders, this);
    }
    
    std::cout << "Matching engine started with " << num_threads << " worker threads" << std::endl;
    return true;
}

void MatchingEngine::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Wait for all worker threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
    
    std::cout << "Matching engine stopped" << std::endl;
}

void MatchingEngine::process_orders() {
    while (running_.load()) {
        // Get order from queue (non-blocking)
        auto order = order_queue_.pop();
        if (!order) {
            // No orders available, sleep briefly
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            continue;
        }
        
        // Get or create order book
        auto order_book = get_or_create_order_book(order->symbol);
        
        // Process order and get trades
        auto trades = order_book->add_order(order);
        
        // Handle trades
        for (const auto& trade : trades) {
            handle_trade(trade);
        }
        
        // Update statistics
        orders_processed_.fetch_add(1);
    }
}

std::shared_ptr<OrderBook> MatchingEngine::get_or_create_order_book(const std::string& symbol) {
    std::unique_lock<std::shared_mutex> lock(engine_mutex_);
    
    auto it = order_books_.find(symbol);
    if (it == order_books_.end()) {
        auto order_book = std::make_shared<OrderBook>(symbol);
        order_books_[symbol] = order_book;
        return order_book;
    }
    
    return it->second;
}

void MatchingEngine::handle_trade(const Trade& trade) {
    trades_executed_.fetch_add(1);
    total_volume_.fetch_add(trade.quantity);
    
    // In a real system, this would:
    // 1. Send trade notifications
    // 2. Update positions
    // 3. Persist to database
    // 4. Send market data updates
    
    std::cout << "Trade: " << trade.symbol << " " << trade.quantity 
              << " @ " << trade.price << " (Buy: " << trade.buy_order_id
              << ", Sell: " << trade.sell_order_id << ")" << std::endl;
}

std::vector<Trade> MatchingEngine::add_order(uint64_t order_id, const std::string& symbol, 
                                            bool is_buy, uint64_t quantity, uint64_t price) {
    auto order = std::make_shared<Order>(order_id, quantity, price, is_buy, symbol);
    
    // Add to processing queue
    while (!order_queue_.push(order.get())) {
        // Queue full, wait briefly
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
    
    // For now, process immediately (simplified)
    // In a real system, this would be async
    auto order_book = get_or_create_order_book(symbol);
    return order_book->add_order(order);
}

bool MatchingEngine::cancel_order(uint64_t order_id) {
    // This is a simplified implementation
    // In a real system, we'd need to find the order book containing this order
    std::shared_lock<std::shared_mutex> lock(engine_mutex_);
    
    for (auto& [symbol, order_book] : order_books_) {
        if (order_book->cancel_order(order_id)) {
            return true;
        }
    }
    
    return false;
}

bool MatchingEngine::modify_order(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) {
    // This is a simplified implementation
    // In a real system, we'd need to find the order book containing this order
    std::shared_lock<std::shared_mutex> lock(engine_mutex_);
    
    for (auto& [symbol, order_book] : order_books_) {
        if (order_book->modify_order(order_id, new_quantity, new_price)) {
            return true;
        }
    }
    
    return false;
}

uint64_t MatchingEngine::get_best_bid(const std::string& symbol) const {
    std::shared_lock<std::shared_mutex> lock(engine_mutex_);
    
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second->get_best_bid();
    }
    
    return 0;
}

uint64_t MatchingEngine::get_best_ask(const std::string& symbol) const {
    std::shared_lock<std::shared_mutex> lock(engine_mutex_);
    
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second->get_best_ask();
    }
    
    return 0;
}

uint64_t MatchingEngine::get_spread(const std::string& symbol) const {
    std::shared_lock<std::shared_mutex> lock(engine_mutex_);
    
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second->get_spread();
    }
    
    return 0;
}

std::vector<std::string> MatchingEngine::get_symbols() const {
    std::shared_lock<std::shared_mutex> lock(engine_mutex_);
    
    std::vector<std::string> symbols;
    symbols.reserve(order_books_.size());
    
    for (const auto& [symbol, order_book] : order_books_) {
        symbols.push_back(symbol);
    }
    
    return symbols;
}

size_t MatchingEngine::get_order_count(const std::string& symbol) const {
    std::shared_lock<std::shared_mutex> lock(engine_mutex_);
    
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second->get_order_count();
    }
    
    return 0;
}

} // namespace trading
