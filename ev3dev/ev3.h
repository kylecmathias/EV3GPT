#ifndef EV3_H
#define EV3_H

#include "config.h"
#include "connection.h"
#include "robot.h"
#include "tasks.h"

std::queue<MotorCommand> cmd_queue;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
std::atomic<bool> running(true);
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

int test_connection(const int sockfd);
void signal_handler(int signum);

#endif /* #ifndef EV3_H */