#include "tasks.h"

extern std::queue<MotorCommand> cmd_queue;
extern std::atomic<bool> running;
extern pthread_mutex_t queue_mutex;

struct timespec sleep_time = {0, 10000};

void* motor_listener(void* arg) {
    int sockfd = *(int*)arg;
    MotorCommand incoming;
    while (running) {
        if (receive_motor_packet(sockfd, &incoming)) {
            pthread_mutex_lock(&queue_mutex);

            if (cmd_queue.size() >= MOTOR_COMMAND_QUEUE_SIZE) cmd_queue.pop();
            cmd_queue.push(incoming);
            pthread_mutex_unlock(&queue_mutex);
        }
        nanosleep(&sleep_time, NULL);
    }
    return NULL;
}

void* motor_runner(void* arg) {
    Robot* ev3 = (Robot*)arg;
    MotorCommand current;
    while (running) {
        pthread_mutex_lock(&queue_mutex);
        if (!cmd_queue.empty()) {
            current = cmd_queue.front();
            cmd_queue.pop();

            pthread_mutex_unlock(&queue_mutex);
            
            ev3->executeMotorCommands(current);
        }
        else pthread_mutex_unlock(&queue_mutex);

        

        nanosleep(&sleep_time, NULL);
    }
    return NULL;
}