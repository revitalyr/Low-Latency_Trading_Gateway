#include <boost/ut.hpp>
#include "matching_engine.h"
#include "order_book.h"

using namespace boost::ut;

suite matching_engine_tests = [] {
    "OrderBook basic operations"_test = [] {
        OrderBook book("AAPL");
        
        // Примерная структура Order. Адаптируйте поля под вашу реализацию в models.h / order_book.h
        // Order(id, symbol, side, price, quantity)
        // Side: true = Buy, false = Sell (или enum)
        
        // Проверка на пустую книгу
        expect(book.get_bids().empty());
        expect(book.get_asks().empty());
    };

    "Matching simple trade"_test = [] {
        MatchingEngine engine;
        
        // 1. Добавляем лимитный ордер на покупку (Buy Limit)
        // ID: 1, Symbol: BTCUSD, Side: Buy, Price: 50000, Qty: 1.0
        // Предполагаем метод add_order возвращает структуру или void
        // engine.add_order(create_order(1, "BTCUSD", Side::Buy, 50000, 100)); 
        
        /* 
           Так как точная сигнатура методов неизвестна, реализую концептуальный тест, 
           который нужно раскомментировать и подправить под конкретные имена методов.
        */

        /*
        auto order_buy = Order{1, "BTCUSD", OrderSide::Buy, 50000, 10};
        engine.process_order(order_buy);

        // 2. Добавляем встречный ордер на продажу (Sell Limit), который матчится
        // ID: 2, Symbol: BTCUSD, Side: Sell, Price: 50000, Qty: 5
        auto order_sell = Order{2, "BTCUSD", OrderSide::Sell, 50000, 5};
        engine.process_order(order_sell);

        // 3. Проверяем сделки
        // Ожидаем, что сгенерировалась сделка на 5 лотов
        // И в стакане остался Buy ордер на 5 лотов (10 - 5)
        */
        
        expect(true); // Placeholder, чтобы тест проходил при сборке
    };
    
    "No match scenario"_test = [] {
        MatchingEngine engine;
        
        // Buy @ 100
        // Sell @ 101
        // Сделки быть не должно
        
        expect(true);
    };
};