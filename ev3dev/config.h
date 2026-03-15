#ifndef CONFIG_H
#define CONFIG_H

#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#if defined(__linux__)
#include <unistd.h>
#include <pthread.h>
#endif /* #if defined(__linux__) */

#include <iostream>
#include <string>
#include <atomic>
#include <queue>
#include <stdexcept>
#include <fstream>
#include <cstring>

#define MOTOR_COMMAND_QUEUE_SIZE 3
#define SLEEP_TIME_U 10000
#define JETSON_ADDRESS "10.100.5.22" 
#define JETSON_PORT 43137

#endif /* #ifndef CONFIG_H */