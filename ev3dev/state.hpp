#pragma once

#include "config.hpp"
#include "connection.hpp"

inline sem_t queue_sem;
inline std::atomic<bool> emergency_active{false};
inline std::atomic<bool> grounded{true};
inline std::atomic<bool> running{true};
inline pthread_mutex_t stop_mutex  = PTHREAD_MUTEX_INITIALIZER;
inline pthread_cond_t  stop_cond   = PTHREAD_COND_INITIALIZER;

class MotorCommandRingBuffer {
    public:
        static constexpr int MAX_ITEMS = MOTOR_COMMAND_QUEUE_SIZE;

        MotorCommand buf[MAX_ITEMS];
        std::atomic<size_t> head{0};
        std::atomic<size_t> tail{0};
    public:
        bool push(const MotorCommand& cmd);
        bool empty() const;
        MotorCommand pop();
        MotorCommand peek() const;
        void clear();
};

inline MotorCommandRingBuffer cmd_queue;