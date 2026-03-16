#include "ev3.h"

int connect(const int sockfd) {
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

    struct sockaddr_in jetson_addr;
    int sockfd = init_connection(&jetson_addr, JETSON_ADDRESS, RECV_PORT);

    if (connect(sockfd) == -1) throw std::runtime_error("Connection to jetson timed out");

    pthread_t listener_tid, motor_runner_tid;
    pthread_create(&listener_tid, NULL, motor_listener, &sockfd);
    pthread_create(&motor_runner_tid, NULL, motor_runner, &ev3);

    uint8_t timestamp = 0;

    while (running) {
        SensorPacket pkt;

        pkt.header = 0x00 | (PORT_MASK & (ev3.getMask(SENSOR) << PORT_SHIFT)) | (GYRO_MASK & (GYRO_MODE << GYRO_SHIFT));
        pkt.ultrasonic = (uint8_t) ev3.readSensor(SENSOR_3);
        pkt.color = (uint8_t) static_cast<ColorID>(ev3.readSensor(COLOR));
        pkt.ir_prox = (uint8_t) ev3.readSensor(INFRARED, IR_PROX);
        pkt.ir_angle = (int8_t) ev3.readSensor(INFRARED, IR_ANGLE);
        pkt.timestamp = timestamp;

        pack_sensor_data(&pkt, (float) ev3.readSensor(GYRO));
        send_sensor_packet(sockfd, &pkt, &jetson_addr);
        timestamp++;
        usleep(SLEEP_TIME_U);
    }
}