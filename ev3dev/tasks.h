#ifndef TASKS_H
#define TASKS_H

#include "config.h"
#include "connection.h"
#include "robot.h"

void* motor_listener(void* arg);
void* motor_runner(void* arg);
void* emergency_reverse(void* arg);

#define THREAD_SLEEP_U 100000

#endif /* #ifndef TASKS_H */