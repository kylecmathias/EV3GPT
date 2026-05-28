#include "tasks.h"

extern std::queue<MotorCommand> cmd_queue;
extern std::atomic<bool> running;
extern std::atomic<bool> grounded;
extern pthread_mutex_t queue_mutex;
extern pthread_cond_t queue_cond;
extern pthread_cond_t stop_cond;

std::atomic<bool> ignore(false);
struct timespec sleep_time = {0, 50000};

void* motor_listener(void* arg) {
    int sockfd = *(int*)arg;
    MotorCommand incoming;

    while (running) {
        if (receive_motor_packet(sockfd, &incoming)) {
            pthread_mutex_lock(&queue_mutex);

            if (!ignore.load() || incoming.stop == EMERGENCY_STOP) {
                if (cmd_queue.size() >= MOTOR_COMMAND_QUEUE_SIZE) cmd_queue.pop();
                cmd_queue.push(incoming);
                pthread_cond_signal(&queue_cond);
            }

            pthread_mutex_unlock(&queue_mutex);
        }

        usleep(1000);
    }
    return NULL;
}

void* motor_runner(void* arg) {
    Robot* ev3 = (Robot*)arg;
    MotorCommand current;

    while (running) {
        pthread_mutex_lock(&queue_mutex);

        while (cmd_queue.empty() && running.load()) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }

        if (!running.load()) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        current = cmd_queue.front();
        cmd_queue.pop();

        if (!current.sync) {
            ignore = true;
            while (!cmd_queue.empty()) cmd_queue.pop();
        }

        pthread_mutex_unlock(&queue_mutex);

        try {
            ev3->executeMotorCommands(current);
        }
        catch (...) {
            std::cout << "Error executing commands\n";
        }

        ignore = false;
    }
    return NULL;
}

void* emergency_reverse(void* arg) {
    Robot* ev3 = (Robot*)arg;

    while (running) {
        pthread_mutex_lock(&queue_mutex);

        while (grounded.load() && running.load()) {
            pthread_cond_wait(&stop_cond, &queue_mutex);
        }

        if (!running.load()) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        ignore.store(true);
        while (!cmd_queue.empty()) cmd_queue.pop();
        pthread_mutex_unlock(&queue_mutex);

        MotorCommand cmd = REVERSE_COMMAND;
        ev3->executeMotorCommands(cmd);

        cmd = (MotorCommand)STOP_COMMAND;
        ev3->executeMotorCommands(cmd);

        while (!grounded.load()) usleep(SLEEP_TIME_U);

        ignore.store(false);
    }
    return NULL;
}