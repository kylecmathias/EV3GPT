#ifndef CONFIG_H
#define CONFIG_H

#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

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

#define MOTOR_COMMAND_QUEUE_SIZE 8
#define SLEEP_TIME_U 10000
#define SLOWDOWN 1

#define IGNORE_NO_SYNC false
#define EMERGENCY_STOP 0x03
#define EMERGENCY_THREAD_PRIORITY 90

#define DIV_NS 1e9f
#define MUL_MS 1e3f

#define JETSON_ADDRESS "jetson.local" 
#define RECV_PORT 8888

#define AUDIO_PORT 8889
#define AUDIO_PAYLOAD_SIZE 512
#define AUDIO_PACKET_SIZE (AUDIO_PAYLOAD_SIZE + 1)
#define AUDIO_SAMPLE_RATE 8000
#define AUDIO_CHANNELS 1

#define AUDIO_PRIORITY_MASK  0xE0U  //0b1110 0000 - msb 3 bits
#define AUDIO_PRIORITY_SHIFT 5
#define AUDIO_FLAG_INTERRUPT 0x10U  //0b0001 0000
#define AUDIO_FLAG_FLUSH     0x08U  //0b0000 1000
#define AUDIO_FLAG_START     0x04U  //0b0000 0100
#define AUDIO_FLAG_END       0x02U  //0b0000 0010
#define AUDIO_FLAG_RESERVED  0x01U  //0b0000 0001

#endif /* #ifndef CONFIG_H */