#include "connection.h"

uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = CRC_INIT;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }
    return crc;
}

void pack_sensor_data(SensorPacket *packet, float gyro) {
    int16_t gyro_fp = (int16_t)(gyro * GYRO_FP);

    packet->gyro = htons(gyro_fp);
    
    packet->crc = crc8((uint8_t*)packet, PAYLOAD_SIZE);
}

bool receive_motor_packet(int sockfd, MotorCommand *command) {
    MotorPacket packet;

    struct sockaddr_in jetson_addr;
    memset(&jetson_addr, 0, sizeof(jetson_addr));

    socklen_t len = sizeof(jetson_addr);
    
    ssize_t recv_len = recvfrom(sockfd, &packet, sizeof(packet), MSG_DONTWAIT, (struct sockaddr *) &jetson_addr, &len);
    
    if (recv_len == sizeof(MotorPacket)) {
        if (!(packet.header & (1 << 7))) return false;
        uint8_t calculated_crc = crc8((uint8_t *)&packet, PAYLOAD_SIZE);
        
        if (calculated_crc == packet.crc) {
            *command = unpack_motor_data(&packet);
            return true;
        }
    }
    return false;
}

MotorCommand unpack_motor_data(const MotorPacket *packet) {
    MotorCommand command;

    command.ports = (packet->header & PORT_MASK) >> MTR_SHIFT;
    command.unit = (packet->header & UNIT_MASK) >> UNIT_SHIFT;
    command.sync = (packet->header & SYNC_MASK);
    
    command.speeds[MOTOR_A] = packet->motor_a;
    command.speeds[MOTOR_B] = packet->motor_b;
    command.speeds[MOTOR_C] = packet->motor_c;
    command.speeds[MOTOR_D] = packet->motor_d;
    command.stop = packet->stop;

    command.duration = ntohs(packet->duration);

    return command;
}

void send_sensor_packet(int sockfd, const SensorPacket *packet, struct sockaddr_in *dest_addr) {
    sendto(sockfd, packet, sizeof(SensorPacket), 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr));
}

int init_connection(struct sockaddr_in *dest_addr, const char *ip, uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return -1;

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY; 
    local_addr.sin_port = htons(port);       

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return -1;
    }

    dest_addr->sin_family = AF_INET;
    dest_addr->sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &dest_addr->sin_addr) <= 0) {
        return -1;
    }

    return sockfd;
}