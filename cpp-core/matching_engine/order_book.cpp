#include "order_book.h"
#include <algorithm>

namespace trading {

OrderBook::OrderBook(Symbol symbol) : m_symbol(std::move(symbol)) {}

void OrderBook::add_order(OrderPtr order) {
    if (order->is_buy) {
        m_bids.push_back(order);
        std::sort(m_bids.begin(), m_bids.end(), [](const auto& a, const auto& b) {
            return a->price > b->price;
        });
    } else {
        m_asks.push_back(order);
        std::sort(m_asks.begin(), m_asks.end(), [](const auto& a, const auto& b) {
            return a->price < b->price;
        });
    }
}

OrderBook::Trades OrderBook::match() {
    Trades trades;
    
    while (!m_bids.empty() && !m_asks.empty()) {
        auto& best_bid = m_bids.front();
        auto& best_ask = m_asks.front();
        
        if (best_bid->price >= best_ask->price) {
            uint64_t quantity = std::min(best_bid->quantity, best_ask->quantity);
            
            // Теперь это работает, так как в Trade есть конструктор принимающий std::string
            trades.emplace_back(
                best_bid->id,
                best_ask->id,
                best_ask->price,
                quantity,
                m_symbol
            );
            
            best_bid->quantity -= quantity;
            best_ask->quantity -= quantity;
            
            if (best_bid->quantity == 0) m_bids.erase(m_bids.begin());
            if (best_ask->quantity == 0) m_asks.erase(m_asks.begin());
        } else {
            break;
        }
    }
    return trades;
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto remove_pred = [order_id](const OrderPtr& o) {
        return o->id == order_id;
    };

    // Пытаемся удалить из bids
    auto it_bid = std::remove_if(m_bids.begin(), m_bids.end(), remove_pred);
    if (it_bid != m_bids.end()) {
        m_bids.erase(it_bid, m_bids.end());
        return true;
    }

    // Если не нашли, пытаемся удалить из asks
    auto it_ask = std::remove_if(m_asks.begin(), m_asks.end(), remove_pred);
    if (it_ask != m_asks.end()) {
        m_asks.erase(it_ask, m_asks.end());
        return true;
    }

    return false;
}

bool OrderBook::modify_order(uint64_t order_id, uint32_t new_quantity, uint64_t new_price) {
    // Для простоты реализации: ищем ордер, удаляем его и добавляем новый с новыми параметрами.
    // Это гарантирует правильную сортировку, хотя и теряет приоритет времени.
    
    OrderPtr found_order = nullptr;

    // Вспомогательная лямбда для поиска и удаления
    auto find_and_remove = [&](Orders& orders) -> bool {
        auto it = std::find_if(orders.begin(), orders.end(), 
            [order_id](const auto& o) { return o->id == order_id; });
        
        if (it != orders.end()) {
            found_order = *it; // Копируем указатель
            orders.erase(it);
            return true;
        }
        return false;
    };

    if (!find_and_remove(m_bids)) {
        find_and_remove(m_asks);
    }

    if (found_order) {
        // Обновляем поля
        found_order->quantity = new_quantity;
        found_order->price = new_price;
        // Добавляем обратно (вызовет сортировку)
        add_order(found_order);
        return true;
    }
    return false;
}

}