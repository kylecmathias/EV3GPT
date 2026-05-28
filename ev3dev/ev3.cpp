#include "ev3.h"

int test_connection(const int sockfd) {
    std::cout << "Connecting to jetson" << std::endl;

    MotorCommand handshake;
    struct timespec start, current;

    clock_gettime(CLOCK_MONOTONIC, &start);

    while (!receive_motor_packet(sockfd, &handshake)) {
        clock_gettime(CLOCK_MONOTONIC, &current);

        double elapsed = (current.tv_sec - start.tv_sec) + (current.tv_nsec - start.tv_nsec) / DIV_NS;
        if (elapsed > CONNECT_TIMEOUT) return CONNECT_FAIL;
        
        usleep(THREAD_SLEEP_U);
    }

    std::cout << "Connected to jetson" << std::endl;

    return CONNECT_SUCCESS;
}

int main() {
    Robot ev3;
    ev3.initSensor(COLOR, SensorModes::Color);
    ev3.initSensor(GYRO, SensorModes::Gyro);
    ev3.initSensor(ULTRASONIC, SensorModes::Ultrasonic);
    ev3.initSensor(INFRARED, SensorModes::Infrared);

    ev3.resetGyro();

    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setprotocol(&mutex_attr, PTHREAD_PRIO_INHERIT);
    pthread_mutex_init(&queue_mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);

    struct sockaddr_in jetson_addr;
    int sockfd = init_connection(&jetson_addr, JETSON_ADDRESS, RECV_PORT);

    if (test_connection(sockfd) == CONNECT_FAIL) throw std::runtime_error("Connection to jetson timed out");

    pthread_t listener_tid, motor_runner_tid, emergency_reverse_tid;
    pthread_create(&listener_tid, NULL, motor_listener, &sockfd);
    pthread_create(&motor_runner_tid, NULL, motor_runner, &ev3);
    pthread_create(&emergency_reverse_tid, NULL, emergency_reverse, &ev3);

    struct sched_param param;
    param.sched_priority = EMERGENCY_THREAD_PRIORITY;
    if (pthread_setschedparam(emergency_reverse_tid, SCHED_FIFO, &param) != 0) {
        std::cerr << "Failed to set realtime priority for emergency_reverse" << std::endl;
    }

    pthread_t audio_receiver_tid, audio_player_tid;
    pthread_create(&audio_receiver_tid, NULL, audio_receiver, NULL);
    pthread_create(&audio_player_tid, NULL, audio_player, NULL);

    uint8_t timestamp = 0;
    SensorPacket pkt;

    while (running.load()) {
        pkt.header = 0x00 | (PORT_MASK & (ev3.getMask(SENSOR) << PORT_SHIFT)) | (GYRO_MASK & (GYRO_MODE << GYRO_SHIFT));
        pkt.ultrasonic = (uint8_t) ev3.readSensor(ULTRASONIC);
        pkt.color = (uint8_t) (ev3.readSensor(COLOR));
        pkt.ir_prox = (uint8_t) ev3.readSensor(INFRARED, IR_PROX);
        pkt.ir_angle = (int8_t) ev3.readSensor(INFRARED, IR_ANGLE);
        pkt.timestamp = timestamp;

        if (pkt.color <= GROUNDED_THRESHOLD) {
            pthread_mutex_lock(&queue_mutex);
            grounded = false;
            pthread_cond_signal(&stop_cond);
            pthread_mutex_unlock(&queue_mutex);
        }
        else {
            grounded = true;
        }

        pack_sensor_data(&pkt, (float) ev3.readSensor(GYRO));
        send_sensor_packet(sockfd, &pkt);
        timestamp++;
        for (int i = 0; i < SLOWDOWN; i++) usleep(SLEEP_TIME_U);
    }

    pthread_join(listener_tid, NULL);
    pthread_join(motor_runner_tid, NULL);
    pthread_join(emergency_reverse_tid, NULL);
    pthread_join(audio_receiver_tid, NULL);
    pthread_join(audio_player_tid, NULL);

    pthread_mutex_destroy(&queue_mutex);
    close(sockfd);
}