#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>

namespace trading {

template<typename T, size_t Size>
class LockFreeQueue {
private:
    struct Node {
        // Store T directly. If T is Order*, this will be atomic<Order*>
        using AtomicData = std::atomic<T>;
        using AtomicNext = std::atomic<Node*>;
        AtomicData m_data{nullptr}; 
        AtomicNext m_next{nullptr};
    };

    using AtomicNodePtr = std::atomic<Node*>;
    using AtomicIndex = std::atomic<size_t>;

    alignas(64) AtomicNodePtr m_head;
    alignas(64) AtomicNodePtr m_tail;
    Node* m_nodes;
    AtomicIndex m_write_pos{0};
    AtomicIndex m_read_pos{0};

public:
    LockFreeQueue() {
        m_nodes = static_cast<Node*>(operator new[](Size * sizeof(Node)));
        for (size_t i = 0; i < Size; ++i) {
            new (&m_nodes[i]) Node();
        }
        
        m_head.store(&m_nodes[0]);
        m_tail.store(&m_nodes[0]);
        
        for (size_t i = 0; i < Size - 1; ++i) {
            m_nodes[i].m_next.store(&m_nodes[i + 1]);
        }
        m_nodes[Size - 1].m_next.store(nullptr);
    }

    ~LockFreeQueue() {
        T data;
        while ((data = pop()) != nullptr) {
            if constexpr (std::is_pointer_v<T>) {
                delete data;
            }
        }
        for (size_t i = 0; i < Size; ++i) {
            m_nodes[i].~Node();
        }
        operator delete[](m_nodes);
    }

    // Now accepts T (i.e., Order*), not T* (Order**)
    bool push(T item) {
        size_t current_write = m_write_pos.load(std::memory_order_relaxed);
        size_t current_read = m_read_pos.load(std::memory_order_acquire);
        
        if ((current_write + 1) % Size == current_read) return false;
        
        Node* tail = m_tail.load(std::memory_order_relaxed);
        // Use current node for data to avoid issues with 'next'
        tail->m_data.store(item, std::memory_order_release);
        
        Node* next = tail->m_next.load(std::memory_order_relaxed);
        if (next) {
            m_tail.store(next, std::memory_order_release);
            m_write_pos.store((current_write + 1) % Size, std::memory_order_release);
            return true;
        }
        return false;
    }

    T pop() {
        Node* head = m_head.load(std::memory_order_relaxed);
        T data = head->m_data.load(std::memory_order_acquire);
        
        if (data == nullptr) return nullptr;
        
        Node* next = head->m_next.load(std::memory_order_relaxed);
        if (next == nullptr) return nullptr;
        
        head->m_data.store(nullptr, std::memory_order_release);
        m_head.store(next, std::memory_order_release);
        
        size_t current_read = m_read_pos.load(std::memory_order_relaxed);
        m_read_pos.store((current_read + 1) % Size, std::memory_order_release);
        
        return data;
    }
    
    size_t size() const {
        size_t w = m_write_pos.load(std::memory_order_relaxed);
        size_t r = m_read_pos.load(std::memory_order_relaxed);
        if (w >= r) return w - r;
        return Size - (r - w);
    }

    bool empty() const {
        return m_write_pos.load(std::memory_order_acquire) == m_read_pos.load(std::memory_order_acquire);
    }

    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
};

} // namespace trading