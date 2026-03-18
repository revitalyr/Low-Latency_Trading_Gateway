#include "lockfree_queue.h"

namespace trading {

template<typename T, size_t Size>
LockFreeQueue<T, Size>::LockFreeQueue() {
    nodes_ = static_cast<Node*>(operator new[](Size * sizeof(Node)));
    
    // Initialize all nodes
    for (size_t i = 0; i < Size; ++i) {
        new (&nodes_[i]) Node();
    }
    
    head_.store(&nodes_[0]);
    tail_.store(&nodes_[0]);
    
    // Link all nodes
    for (size_t i = 0; i < Size - 1; ++i) {
        nodes_[i].next.store(&nodes_[i + 1]);
    }
    nodes_[Size - 1].next.store(nullptr);
}

template<typename T, size_t Size>
LockFreeQueue<T, Size>::~LockFreeQueue() {
    // Clean up any remaining data
    T* data;
    while ((data = pop()) != nullptr) {
        delete data;
    }
    
    // Destroy nodes
    for (size_t i = 0; i < Size; ++i) {
        nodes_[i].~Node();
    }
    
    operator delete[](nodes_);
}

template<typename T, size_t Size>
bool LockFreeQueue<T, Size>::push(T* item) {
    size_t current_write = write_pos_.load(std::memory_order_relaxed);
    size_t current_read = read_pos_.load(std::memory_order_acquire);
    
    // Check if queue is full
    if ((current_write + 1) % Size == current_read) {
        return false;
    }
    
    Node* tail = tail_.load(std::memory_order_relaxed);
    Node* next = tail->next.load(std::memory_order_relaxed);
    
    if (next == nullptr) {
        return false; // Queue full
    }
    
    // Store data in the next node
    next->data.store(item, std::memory_order_release);
    
    // Advance tail
    tail_.store(next, std::memory_order_release);
    write_pos_.store((current_write + 1) % Size, std::memory_order_release);
    
    return true;
}

template<typename T, size_t Size>
T* LockFreeQueue<T, Size>::pop() {
    Node* head = head_.load(std::memory_order_relaxed);
    T* data = head->data.load(std::memory_order_acquire);
    
    if (data == nullptr) {
        return nullptr; // Queue empty
    }
    
    Node* next = head->next.load(std::memory_order_relaxed);
    if (next == nullptr) {
        return nullptr; // Shouldn't happen
    }
    
    // Clear data and advance head
    head->data.store(nullptr, std::memory_order_release);
    head_.store(next, std::memory_order_release);
    
    size_t current_read = read_pos_.load(std::memory_order_relaxed);
    read_pos_.store((current_read + 1) % Size, std::memory_order_release);
    
    return data;
}

template<typename T, size_t Size>
size_t LockFreeQueue<T, Size>::size() const {
    size_t write_pos = write_pos_.load(std::memory_order_acquire);
    size_t read_pos = read_pos_.load(std::memory_order_acquire);
    
    if (write_pos >= read_pos) {
        return write_pos - read_pos;
    } else {
        return Size - (read_pos - write_pos);
    }
}

template<typename T, size_t Size>
bool LockFreeQueue<T, Size>::empty() const {
    return size() == 0;
}

} // namespace trading
