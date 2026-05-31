#include "tasks.hpp"

void* motor_listener(void* arg) {
    int sockfd = *(int*)arg;
    MotorCommand incoming;

    while (running.load(std::memory_order_relaxed)) {
        if (!receive_motor_packet(sockfd, &incoming)) {
            usleep(THREAD_SLEEP_FAST);
            continue;
        }
        if (emergency_active.load(std::memory_order_acquire)) {
            if (incoming.stop != EMERGENCY_STOP) {
                usleep(THREAD_SLEEP_FAST);
                continue;
            }
            emergency_active.store(false, std::memory_order_release);
        }

        if (cmd_queue.push(incoming)) {
            sem_post(&queue_sem);
        } 
        else {
            write(STDOUT_FILENO, "WARNING: Buffer full, packet dropped\n", 34);
        }
 
        usleep(THREAD_SLEEP_FAST);
    }
    return NULL;
}

void* motor_runner(void* arg) {
    Robot* ev3 = (Robot*)arg;

    while (running.load(std::memory_order_relaxed)) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += THREAD_SLEEP_NS; 
        if (ts.tv_nsec >= THREAD_SLEEP_S) {
            ts.tv_sec++;
            ts.tv_nsec -= THREAD_SLEEP_S;
        }
        if (sem_timedwait(&queue_sem, &ts) != 0) continue;
        if (emergency_active.load(std::memory_order_acquire)) continue;
        if (cmd_queue.empty()) continue;
        
        MotorCommand current = cmd_queue.pop();

        if (!current.sync) {
            emergency_active.store(true, std::memory_order_release);
            cmd_queue.clear();
            while (sem_trywait(&queue_sem) == 0) {}
        }

        try {
            ev3->executeMotorCommands(current);
        } catch (...) {
            write(STDOUT_FILENO, "Error executing commands\n", 25);
        }
 
        if (!current.sync) {
            emergency_active.store(false, std::memory_order_release);
        }
    }
    return NULL;
}

void* emergency_reverse(void* arg) {
    Robot* ev3 = (Robot*)arg;

    while (running.load(std::memory_order_relaxed)) {
        pthread_mutex_lock(&stop_mutex);

        while (grounded.load(std::memory_order_acquire) && running.load(std::memory_order_relaxed)) {
            pthread_cond_wait(&stop_cond, &stop_mutex);
        }
        pthread_mutex_unlock(&stop_mutex);

        if (!running.load(std::memory_order_relaxed)) break;

        emergency_active.store(true, std::memory_order_release);

        sched_yield();

        cmd_queue.clear();
        while (sem_trywait(&queue_sem) == 0) {}
 
        MotorCommand rev_cmd = REVERSE_COMMAND;
        ev3->executeMotorCommands(rev_cmd);
 
        MotorCommand stop_cmd = STOP_COMMAND;
        ev3->executeMotorCommands(stop_cmd);

        while (!grounded.load(std::memory_order_acquire) && running.load(std::memory_order_relaxed)) {
            usleep(SLEEP_TIME_U);
        }
 
        emergency_active.store(false, std::memory_order_release);
    }
    return NULL;
}