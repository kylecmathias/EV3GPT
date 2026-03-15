#ifndef EV3_H
#define EV3_H

#include "config.h"
#include "connection.h"
#include "robot.h"
#include "tasks.h"

std::queue<MotorCommand> cmd_queue;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
std::atomic<bool> running(true);

int connect(const int sockfd);

#endif /* #ifndef EV3_H */