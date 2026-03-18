#pragma once

#include "order_book.h"
#include "../lockfree_queue/lockfree_queue.h"
#include <unordered_map>
#include <thread>
#include <atomic>
#include <string>
#include <memory>
#include <expected>
#include <string>
#include <functional>
#include "../network/statsd_client.h"

namespace trading {

class MatchingEngine {
public:
    using StartResult = std::expected<void, std::string>;
    using OnTradeCallback = std::function<void(const Trade&)>;

    MatchingEngine();
    ~MatchingEngine();

    [[nodiscard]] StartResult start();
    void stop();
    void add_order(const Order& order);
    bool cancel_order(uint64_t order_id);
    bool modify_order(uint64_t order_id, uint32_t new_quantity, uint64_t new_price);
    
    void set_trade_callback(OnTradeCallback callback);

private:
    void process_orders();

    using AtomicBool = std::atomic<bool>;
    using WorkerThread = std::thread;
    using OrderQueue = LockFreeQueue<Order*, 10000>;
    using Symbol = std::string;
    using BookPtr = std::unique_ptr<OrderBook>;
    using OrderBooks = std::unordered_map<Symbol, BookPtr>;
    using OrderSymbolMap = std::unordered_map<uint64_t, Symbol>;
    
    AtomicBool m_running{false};
    WorkerThread m_worker_thread;
    OrderQueue m_order_queue;
    OrderQueue m_free_list; // Reusable object pool
    OrderBooks m_order_books;
    OrderSymbolMap m_order_id_to_symbol;
    OnTradeCallback m_trade_callback;
    StatsDClient m_statsd;
};

}