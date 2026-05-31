#pragma once

#include "config.hpp"
#include "connection.hpp"
#include "robot.hpp"
#include "state.hpp"

void* motor_listener(void* arg);
void* motor_runner(void* arg);
void* emergency_reverse(void* arg);

constexpr uint32_t THREAD_SLEEP_U = 100000;
constexpr uint32_t THREAD_SLEEP_FAST = 1000;
constexpr uint32_t THREAD_SLEEP_NS = 20000000; //20ms
constexpr uint32_t THREAD_SLEEP_S = 1000000000; //1s