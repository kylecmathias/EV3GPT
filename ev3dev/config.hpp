#pragma once

#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <semaphore.h>

#if defined(__linux__)
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#endif /* #if defined(__linux__) */

#include <iostream>
#include <string>
#include <atomic>
#include <queue>
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <condition_variable>
#include <cstdint>

inline constexpr size_t MOTOR_COMMAND_QUEUE_SIZE = 8;
inline constexpr uint32_t SLEEP_TIME_U = 10000;
inline constexpr uint8_t SLOWDOWN = 1;

inline constexpr bool IGNORE_NO_SYNC = false;
inline constexpr uint8_t EMERGENCY_STOP = 0x03;
inline constexpr uint8_t EMERGENCY_THREAD_PRIORITY = 90;

inline constexpr float DIV_NS = 1e9f;
inline constexpr float MUL_MS = 1e3f;

inline constexpr const char JETSON_ADDRESS[] = "10.0.0.47";
inline constexpr uint16_t RECV_PORT = 8888;

inline constexpr uint16_t AUDIO_PORT = 8889;
inline constexpr size_t AUDIO_PAYLOAD_SIZE = 512;
inline constexpr size_t AUDIO_PACKET_SIZE = AUDIO_PAYLOAD_SIZE + 1;
inline constexpr uint16_t AUDIO_SAMPLE_RATE = 8000;
inline constexpr size_t AUDIO_CHANNELS = 1;

inline constexpr uint8_t AUDIO_PRIORITY_MASK = 0xE0;  //0b1110 0000 - msb 3 bits
inline constexpr uint8_t AUDIO_PRIORITY_SHIFT = 5;
inline constexpr uint8_t AUDIO_FLAG_INTERRUPT = 0x10;  //0b0001 0000
inline constexpr uint8_t AUDIO_FLAG_FLUSH = 0x08;  //0b0000 1000
inline constexpr uint8_t AUDIO_FLAG_START = 0x04;  //0b0000 0100
inline constexpr uint8_t AUDIO_FLAG_END = 0x02;  //0b0000 0010
inline constexpr uint8_t AUDIO_FLAG_RESERVED = 0x01;  //0b0000 0001

inline constexpr uint8_t MOTOR_A = 0; //right medium motor (claw)
inline constexpr uint8_t MOTOR_B = 1; //right tread large motor 
inline constexpr uint8_t MOTOR_C = 2; //left tread large motor 
inline constexpr uint8_t MOTOR_D = 3; //left medium motor (shoot)
inline constexpr uint8_t SENSOR_1 = 0; //color sensor
inline constexpr uint8_t SENSOR_2 = 1; //gyro sensor
inline constexpr uint8_t SENSOR_3 = 2; //ultrasonic sensor
inline constexpr uint8_t SENSOR_4 = 3; //infrared sensor
