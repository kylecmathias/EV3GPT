#pragma once

#include "config.hpp"

#if defined(__linux__)
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif /* #if defined(__linux__) */

inline constexpr size_t PACKET_SIZE = 9; //includes crc
inline constexpr size_t PAYLOAD_SIZE = PACKET_SIZE - 1; //excludes crc
inline constexpr size_t COMMAND_SIZE = 11;
inline constexpr uint8_t CRC_INIT = 0x00;
inline constexpr uint8_t CONNECT_TIMEOUT = 30;
inline constexpr int8_t CONNECT_SUCCESS = 0;
inline constexpr int8_t CONNECT_FAIL = -1;
inline constexpr int8_t GYRO_FP = 16; //2^4
inline constexpr uint8_t PORT_SHIFT = 6;
inline constexpr uint8_t MTR_SHIFT = 3;
inline constexpr uint8_t GYRO_SHIFT = 1;
inline constexpr uint8_t UNIT_SHIFT = 1;
inline constexpr uint8_t INTERRUPT_SHIFT = 7;
inline constexpr uint8_t PORT_MASK = 0x78; //0b0111 1000 
inline constexpr uint8_t UNIT_MASK = 0x06; //0b0000 0110
inline constexpr uint8_t GYRO_MASK = 0x06; //0b0000 0110
inline constexpr uint8_t STOP_MASK = 0x03; //0b0000 0011
inline constexpr uint8_t SYNC_MASK = 0x01; //0b0000 0001
inline constexpr uint8_t INTERRUPT_MASK = 0x80; //0b1000 0000

#ifndef PACKED
#define PACKED __attribute__((packed))
#endif  /* #ifndef PACKED */

struct PACKED SensorPacket { //structure to hold packets for sensor data without padding
    uint8_t header;
    uint8_t ultrasonic;
    uint8_t color;
    int16_t gyro; //12/4 fixed point, big endian
    uint8_t ir_prox;
    int8_t ir_angle;
    uint8_t timestamp;
    uint8_t crc;
};

struct PACKED MotorPacket { //structure to hold packets for motor data received
    uint8_t header;
    int8_t motor_a;
    int8_t motor_b;
    int8_t motor_c;
    int8_t motor_d;
    uint16_t duration; //big endian
    uint8_t stop;
    uint8_t crc;
};

struct MotorCommand {
    uint8_t unit;
    bool sync;
    bool interrupt;
    uint8_t ports; //which ports to run 0bxxxxABCD
    int8_t speeds[4]; //speeds for motors A, B, C, D
    uint16_t duration; //duration for the motor command in milliseconds
    uint8_t stop; //0 for coast, 1 for brake, 2 for hold
}; //structure to hold motor commands for sending

uint8_t crc8(const uint8_t *data, size_t len); //calculate crc8 checksum for data
void pack_sensor_data(SensorPacket *packet, float gyro); //pack sensor data into a packet for sending
bool receive_motor_packet(int sockfd, MotorCommand *command); //receive packet with motor data
MotorCommand unpack_motor_data(const MotorPacket *packet); //unpack motor data from a received packet and return an array of motor speeds and duration
void send_sensor_packet(int sockfd, const SensorPacket *packet); //send sensor packet to jetson
int init_connection(struct sockaddr_in *dest_addr, const char *ip, uint16_t port); //open a connection with the jetson

static_assert(sizeof(SensorPacket) == PACKET_SIZE, "SensorPacket size mismatch");
static_assert(sizeof(MotorPacket) == PACKET_SIZE, "MotorPacket size mismatch");