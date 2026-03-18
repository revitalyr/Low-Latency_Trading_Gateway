#pragma once

#include <atomic>
#include <memory>
#include <cstddef>

namespace trading {

template<typename T, size_t Size>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<T*> data{nullptr};
        std::atomic<Node*> next{nullptr};
    };

    alignas(64) std::atomic<Node*> head_;
    alignas(64) std::atomic<Node*> tail_;
    Node* nodes_;
    std::atomic<size_t> write_pos_{0};
    std::atomic<size_t> read_pos_{0};

public:
    LockFreeQueue();
    ~LockFreeQueue();

    bool push(T* item);
    T* pop();
    
    size_t size() const;
    bool empty() const;

    // Disable copy and assignment
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
};

} // namespace trading

