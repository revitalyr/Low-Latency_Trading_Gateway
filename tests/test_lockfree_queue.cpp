#include <boost/ut.hpp>
#include <thread>
#include <vector>
#include <atomic>
#include "lockfree_queue.h"

using namespace boost::ut;

// Предполагаем интерфейс LockFreeQueue<T, Size>
// Если ваш класс называется иначе, скорректируйте using или имя класса.

suite lockfree_queue_tests = [] {
    "Queue construction and empty check"_test = [] {
        LockFreeQueue<int, 16> queue;
        expect(queue.empty() >> fatal); // Если очередь не пуста сразу, дальше тестировать нет смысла
    };

    "Simple Enqueue Dequeue"_test = [] {
        LockFreeQueue<int, 16> queue;
        
        expect(queue.push(42));
        expect(not queue.empty());
        
        int value = 0;
        expect(queue.pop(value));
        expect(value == 42_i);
        expect(queue.empty());
    };

    "FIFO Ordering"_test = [] {
        LockFreeQueue<int, 16> queue;
        expect(queue.push(1));
        expect(queue.push(2));
        expect(queue.push(3));

        int v1, v2, v3;
        expect(queue.pop(v1));
        expect(queue.pop(v2));
        expect(queue.pop(v3));

        expect(v1 == 1_i);
        expect(v2 == 2_i);
        expect(v3 == 3_i);
    };

    "Queue full scenario"_test = [] {
        LockFreeQueue<int, 2> queue;
        expect(queue.push(1));
        expect(queue.push(2));
        expect(not queue.push(3)) << "Queue should be full";
    };
};