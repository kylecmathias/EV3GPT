#ifndef EV3_H
#define EV3_H

#include "config.h"
#include "connection.h"
#include "robot.h"
#include "tasks.h"
#include "audio.h"

std::queue<MotorCommand> cmd_queue;
std::atomic<bool> running(true);
std::atomic<bool> grounded(true);

pthread_mutex_t queue_mutex;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

pthread_cond_t stop_cond = PTHREAD_COND_INITIALIZER;

int test_connection(const int sockfd);

#endif /* #ifndef EV3_H */