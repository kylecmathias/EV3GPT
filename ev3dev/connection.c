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
    struct sockaddr_in send_addr;
    socklen_t slen = sizeof(send_addr);

    ssize_t recv_len = recvfrom(sockfd, &packet, sizeof(MotorPacket), MSG_DONTWAIT, (struct sockaddr *)&send_addr, &slen);

    if (recv_len > 0) {
        uint8_t calculated_crc = crc8((uint8_t *)&packet, PAYLOAD_SIZE);
        printf("RECV: len=%ld, head=0x%02X, got_crc=0x%02X, exp_crc=0x%02X\n", (long)recv_len, packet.header, packet.crc, calculated_crc);
    }

    if (recv_len == sizeof(MotorPacket)) {
        if (!(packet.header & (1 << 7))) return false;
        uint8_t calculated_crc = crc8((uint8_t *)&packet, PAYLOAD_SIZE);
        if (calculated_crc == packet.crc) {
            *command = unpack_motor_data(&packet);
            return true;
        }
        else printf("CRC Mismatch: Expected 0x%02X, Got 0x%02X\n", calculated_crc, packet.crc);
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
    local_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all network interfaces
    local_addr.sin_port = htons(port);       // Claim port 43137

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