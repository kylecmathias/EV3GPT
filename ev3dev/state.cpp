#include "state.hpp"

bool MotorCommandRingBuffer::push(const MotorCommand& cmd) {
    size_t t = tail.load(std::memory_order_relaxed);
    size_t next = (t + 1) % MAX_ITEMS;
    if (next == head.load(std::memory_order_acquire)) {
        return false;
    }
    buf[t] = cmd;
    tail.store(next, std::memory_order_release);
    return true;
}

bool MotorCommandRingBuffer::empty() const {
    return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
}

MotorCommand MotorCommandRingBuffer::pop() {
    size_t h = head.load(std::memory_order_relaxed);
    MotorCommand cmd = buf[h];
    head.store((h + 1) % MAX_ITEMS, std::memory_order_release);
    return cmd;
}

MotorCommand MotorCommandRingBuffer::peek() const {
    return buf[head.load(std::memory_order_relaxed)];
}

void MotorCommandRingBuffer::clear() {
    head.store(tail.load(std::memory_order_acquire), std::memory_order_release);
}