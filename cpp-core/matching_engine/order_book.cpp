#include "order_book.h"
#include <algorithm>
#include <iostream>

namespace trading {

OrderBook::OrderBook(const std::string& symbol) : symbol_(symbol) {
}

std::shared_ptr<OrderBook::PriceLevel> OrderBook::find_or_create_price_level(uint64_t price, bool is_buy) {
    auto& levels = is_buy ? buy_levels_ : sell_levels_;
    
    // Search for existing price level
    for (auto& level : levels) {
        if (level->price == price) {
            return level;
        }
    }
    
    // Create new price level
    auto new_level = std::make_shared<PriceLevel>(price);
    
    // Insert in correct order
    if (is_buy) {
        // Buy levels: price descending
        auto it = std::lower_bound(levels.begin(), levels.end(), new_level,
            [](const std::shared_ptr<PriceLevel>& a, const std::shared_ptr<PriceLevel>& b) {
                return a->price > b->price;
            });
        levels.insert(it, new_level);
    } else {
        // Sell levels: price ascending
        auto it = std::lower_bound(levels.begin(), levels.end(), new_level,
            [](const std::shared_ptr<PriceLevel>& a, const std::shared_ptr<PriceLevel>& b) {
                return a->price < b->price;
            });
        levels.insert(it, new_level);
    }
    
    return new_level;
}

void OrderBook::remove_price_level(std::shared_ptr<PriceLevel> level, bool is_buy) {
    auto& levels = is_buy ? buy_levels_ : sell_levels_;
    
    auto it = std::find(levels.begin(), levels.end(), level);
    if (it != levels.end()) {
        levels.erase(it);
    }
}

size_t OrderBook::find_price_level_index(uint64_t price, bool is_buy) const {
    const auto& levels = is_buy ? buy_levels_ : sell_levels_;
    
    for (size_t i = 0; i < levels.size(); ++i) {
        if (levels[i]->price == price) {
            return i;
        }
    }
    
    return static_cast<size_t>(-1);
}

std::vector<Trade> OrderBook::add_order(std::shared_ptr<Order> order) {
    std::vector<Trade> trades;
    std::unique_lock<std::shared_mutex> lock(book_mutex_);
    
    // Store the order
    orders_[order->order_id] = order;
    
    if (order->is_buy) {
        // Buy order - match against sell orders
        while (!sell_levels_.empty() && order->quantity > 0) {
            auto& best_ask = sell_levels_.front();
            
            // Check if we can match
            if (order->price >= best_ask->price) {
                // Match against orders at this price level
                auto& orders = best_ask->orders;
                size_t i = 0;
                
                while (i < orders.size() && order->quantity > 0) {
                    auto& sell_order = orders[i];
                    uint64_t trade_qty = std::min(order->quantity, sell_order->quantity);
                    
                    // Create trade
                    trades.emplace_back(order->order_id, sell_order->order_id, 
                                      trade_qty, best_ask->price, symbol_);
                    
                    // Update quantities
                    order->quantity -= trade_qty;
                    sell_order->quantity -= trade_qty;
                    best_ask->total_quantity -= trade_qty;
                    
                    // Remove fully filled sell order
                    if (sell_order->quantity == 0) {
                        orders_.erase(sell_order->order_id);
                        orders.erase(orders.begin() + i);
                    } else {
                        ++i;
                    }
                }
                
                // Remove empty price level
                if (best_ask->orders.empty()) {
                    remove_price_level(best_ask, false);
                }
            } else {
                break; // No more matches possible
            }
        }
        
        // Add remaining buy order to book
        if (order->quantity > 0) {
            auto level = find_or_create_price_level(order->price, true);
            level->orders.push_back(order);
            level->total_quantity += order->quantity;
        }
    } else {
        // Sell order - match against buy orders
        while (!buy_levels_.empty() && order->quantity > 0) {
            auto& best_bid = buy_levels_.front();
            
            // Check if we can match
            if (order->price <= best_bid->price) {
                // Match against orders at this price level
                auto& orders = best_bid->orders;
                size_t i = 0;
                
                while (i < orders.size() && order->quantity > 0) {
                    auto& buy_order = orders[i];
                    uint64_t trade_qty = std::min(order->quantity, buy_order->quantity);
                    
                    // Create trade
                    trades.emplace_back(buy_order->order_id, order->order_id, 
                                      trade_qty, best_bid->price, symbol_);
                    
                    // Update quantities
                    order->quantity -= trade_qty;
                    buy_order->quantity -= trade_qty;
                    best_bid->total_quantity -= trade_qty;
                    
                    // Remove fully filled buy order
                    if (buy_order->quantity == 0) {
                        orders_.erase(buy_order->order_id);
                        orders.erase(orders.begin() + i);
                    } else {
                        ++i;
                    }
                }
                
                // Remove empty price level
                if (best_bid->orders.empty()) {
                    remove_price_level(best_bid, true);
                }
            } else {
                break; // No more matches possible
            }
        }
        
        // Add remaining sell order to book
        if (order->quantity > 0) {
            auto level = find_or_create_price_level(order->price, false);
            level->orders.push_back(order);
            level->total_quantity += order->quantity;
        }
    }
    
    return trades;
}

bool OrderBook::cancel_order(uint64_t order_id) {
    std::unique_lock<std::shared_mutex> lock(book_mutex_);
    
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }
    
    auto order = it->second;
    bool is_buy = order->is_buy;
    uint64_t price = order->price;
    
    // Find and remove from price level
    size_t level_idx = find_price_level_index(price, is_buy);
    if (level_idx != static_cast<size_t>(-1)) {
        auto& levels = is_buy ? buy_levels_ : sell_levels_;
        auto& level = levels[level_idx];
        
        // Remove order from level
        auto& orders = level->orders;
        auto order_it = std::find(orders.begin(), orders.end(), order);
        if (order_it != orders.end()) {
            level->total_quantity -= order->quantity;
            orders.erase(order_it);
            
            // Remove empty price level
            if (orders.empty()) {
                remove_price_level(level, is_buy);
            }
        }
    }
    
    // Remove from order map
    orders_.erase(it);
    
    return true;
}

bool OrderBook::modify_order(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) {
    std::unique_lock<std::shared_mutex> lock(book_mutex_);
    
    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return false;
    }
    
    auto order = it->second;
    bool is_buy = order->is_buy;
    uint64_t old_price = order->price;
    
    // Remove from old price level
    if (old_price != new_price) {
        size_t level_idx = find_price_level_index(old_price, is_buy);
        if (level_idx != static_cast<size_t>(-1)) {
            auto& levels = is_buy ? buy_levels_ : sell_levels_;
            auto& level = levels[level_idx];
            
            auto& orders = level->orders;
            auto order_it = std::find(orders.begin(), orders.end(), order);
            if (order_it != orders.end()) {
                level->total_quantity -= order->quantity;
                orders.erase(order_it);
                
                // Remove empty price level
                if (orders.empty()) {
                    remove_price_level(level, is_buy);
                }
            }
        }
        
        // Add to new price level
        auto new_level = find_or_create_price_level(new_price, is_buy);
        new_level->orders.push_back(order);
        new_level->total_quantity += new_quantity;
    } else {
        // Same price level, just update quantity
        size_t level_idx = find_price_level_index(old_price, is_buy);
        if (level_idx != static_cast<size_t>(-1)) {
            auto& levels = is_buy ? buy_levels_ : sell_levels_;
            auto& level = levels[level_idx];
            
            level->total_quantity -= order->quantity;
            level->total_quantity += new_quantity;
        }
    }
    
    // Update order
    order->quantity = new_quantity;
    order->price = new_price;
    
    return true;
}

uint64_t OrderBook::get_best_bid() const {
    std::shared_lock<std::shared_mutex> lock(book_mutex_);
    return buy_levels_.empty() ? 0 : buy_levels_.front()->price;
}

uint64_t OrderBook::get_best_ask() const {
    std::shared_lock<std::shared_mutex> lock(book_mutex_);
    return sell_levels_.empty() ? 0 : sell_levels_.front()->price;
}

uint64_t OrderBook::get_bid_size(uint64_t price) const {
    std::shared_lock<std::shared_mutex> lock(book_mutex_);
    
    for (const auto& level : buy_levels_) {
        if (level->price == price) {
            return level->total_quantity;
        }
    }
    return 0;
}

uint64_t OrderBook::get_ask_size(uint64_t price) const {
    std::shared_lock<std::shared_mutex> lock(book_mutex_);
    
    for (const auto& level : sell_levels_) {
        if (level->price == price) {
            return level->total_quantity;
        }
    }
    return 0;
}

size_t OrderBook::get_order_count() const {
    std::shared_lock<std::shared_mutex> lock(book_mutex_);
    return orders_.size();
}

uint64_t OrderBook::get_total_volume() const {
    std::shared_lock<std::shared_mutex> lock(book_mutex_);
    
    uint64_t total = 0;
    for (const auto& level : buy_levels_) {
        total += level->total_quantity;
    }
    for (const auto& level : sell_levels_) {
        total += level->total_quantity;
    }
    return total;
}

uint64_t OrderBook::get_spread() const {
    std::shared_lock<std::shared_mutex> lock(book_mutex_);
    
    if (buy_levels_.empty() || sell_levels_.empty()) {
        return 0;
    }
    
    return sell_levels_.front()->price - buy_levels_.front()->price;
}

} // namespace trading
