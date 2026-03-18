#include "matching_engine.h"
#include <iostream>
#include <thread>
#include <chrono>

#if defined(_MSC_VER)
    #include <intrin.h>
#else
    #include <immintrin.h>
#endif

namespace trading {

MatchingEngine::MatchingEngine() : m_running(false), m_statsd("127.0.0.1", 8125) {
}

MatchingEngine::~MatchingEngine() {
    stop();
}

MatchingEngine::StartResult MatchingEngine::start() {
    if (m_running.load()) {
        return std::unexpected("MatchingEngine already started");
    }
    m_running.store(true);
    m_worker_thread = std::thread(&MatchingEngine::process_orders, this);
    return {};
}

void MatchingEngine::stop() {
    m_running = false;
    if (m_worker_thread.joinable()) {
        m_worker_thread.join();
    }
}

void MatchingEngine::add_order(const Order& order) {
    // Создаем копию ордера в куче и передаем указатель в очередь
    Order* new_order = new Order(order);
    if (!m_order_queue.push(new_order)) {
        std::cerr << "Order queue full!" << std::endl;
        delete new_order;
    }
}

bool MatchingEngine::cancel_order(uint64_t order_id) {
    // Создаем команду отмены
    Order* cmd = new Order();
    cmd->id = order_id;
    cmd->command = OrderCommand::CANCEL;
    
    if (!m_order_queue.push(cmd)) {
        delete cmd;
        return false;
    }
    return true; // Request accepted
}

bool MatchingEngine::modify_order(uint64_t order_id, uint32_t new_quantity, uint64_t new_price) {
    Order* cmd = new Order();
    cmd->id = order_id;
    cmd->quantity = new_quantity;
    cmd->price = new_price;
    cmd->command = OrderCommand::MODIFY;

    if (!m_order_queue.push(cmd)) {
        delete cmd;
        return false;
    }
    return true; // Request accepted
}

void MatchingEngine::set_trade_callback(OnTradeCallback callback) {
    m_trade_callback = std::move(callback);
}

void MatchingEngine::process_orders() {
    while (m_running) {
        Order* order = nullptr;
        if ((order = m_order_queue.pop()) != nullptr) {
            auto start_time = std::chrono::steady_clock::now();
            
            if (order->command == OrderCommand::ADD) {
                Symbol symbol(order->symbol);
                
                // Сохраняем привязку ID к символу для будущих отмен/изменений
                m_order_id_to_symbol[order->id] = symbol;

                if (!m_order_books.contains(symbol)) {
                    m_order_books.emplace(symbol, std::make_unique<OrderBook>(symbol));
                }
                
                auto& book = m_order_books[symbol];
                book->add_order(std::make_shared<Order>(*order));
                
                // Запускаем матчинг и отправляем сделки через колбэк
                auto trades = book->match();
                if (m_trade_callback) {
                    for (const auto& trade : trades) {
                        m_trade_callback(trade);
                    }
                }
            }
            else if (order->command == OrderCommand::CANCEL) {
                if (m_order_id_to_symbol.contains(order->id)) {
                    Symbol symbol = m_order_id_to_symbol[order->id];
                    if (m_order_books.contains(symbol)) {
                        m_order_books[symbol]->cancel_order(order->id);
                    }
                    m_order_id_to_symbol.erase(order->id);
                }
            }
            else if (order->command == OrderCommand::MODIFY) {
                if (m_order_id_to_symbol.contains(order->id)) {
                    Symbol symbol = m_order_id_to_symbol[order->id];
                    if (m_order_books.contains(symbol)) {
                        m_order_books[symbol]->modify_order(order->id, order->quantity, order->price);
                    }
                }
            }

            auto end_time = std::chrono::steady_clock::now();
            auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            
            m_statsd.timing("engine.process_latency_us", duration_us);
            m_statsd.increment("engine.orders_processed");

            // Удаляем команду после обработки (так как она создавалась через new)
            delete order;
        } 
        else {
            std::this_thread::yield();
        }
    }
}

}