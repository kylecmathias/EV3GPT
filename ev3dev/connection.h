#ifndef CONNECTION_H
#define CONNECTION_H

#include "config.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PACKET_SIZE 9 //includes crc
#define PAYLOAD_SIZE (PACKET_SIZE - 1) //excludes crc
#define CRC_INIT 0x00
#define CONNECT_TIMEOUT 30U
#define CONNECT_SUCCESS 1
#define CONNECT_FAIL -1
#define INET_NOFLAG 0
#define GYRO_FP 16 //2^4
#define MOTOR_A 0
#define MOTOR_B 1
#define MOTOR_C 2
#define MOTOR_D 3
#define PORT_SHIFT 6
#define MTR_SHIFT 3
#define GYRO_SHIFT 1
#define UNIT_SHIFT 1
#define INTERRUPT_SHIFT 7
#define PORT_MASK 0x78U //0b0111 1000 
#define UNIT_MASK 0x06U //0b0000 0110
#define GYRO_MASK 0x06U //0b0000 0110
#define STOP_MASK 0x03U //0b0000 0011
#define SYNC_MASK 0x01U //0b0000 0001
#define INTERRUPT_MASK 0x80U //0b1000 0000

#ifndef PACKED
#define PACKED __attribute__((packed))
#endif  /* #ifdef PACKED */

#ifdef __cplusplus
extern "C" {
#endif /* #ifdef __cplusplus */

typedef struct PACKED { //structure to hold packets for sensor data without padding
    uint8_t header;
    uint8_t ultrasonic;
    uint8_t color;
    int16_t gyro; //12/4 fixed point, big endian
    uint8_t ir_prox;
    int8_t ir_angle;
    uint8_t timestamp;
    uint8_t crc;
} SensorPacket;

typedef struct PACKED { //structure to hold packets for motor data received
    uint8_t header;
    int8_t motor_a;
    int8_t motor_b;
    int8_t motor_c;
    int8_t motor_d;
    uint16_t duration; //big endian
    uint8_t stop;
    uint8_t crc;
} MotorPacket;

typedef struct MotorCommand {
    uint8_t unit;
    bool sync;
    bool interrupt;
    uint8_t ports; //which ports to run 0bxxxxABCD
    int8_t speeds[4]; //speeds for motors A, B, C, D
    uint16_t duration; //duration for the motor command in milliseconds
    uint8_t stop; //0 for coast, 1 for brake, 2 for hold
} MotorCommand; //structure to hold motor commands for sending

uint8_t crc8(const uint8_t *data, size_t len); //calculate crc8 checksum for data
void pack_sensor_data(SensorPacket *packet, float gyro); //pack sensor data into a packet for sending
bool receive_motor_packet(int sockfd, MotorCommand *command); //receive packet with motor data
MotorCommand unpack_motor_data(const MotorPacket *packet); //unpack motor data from a received packet and return an array of motor speeds and duration
void send_sensor_packet(int sockfd, const SensorPacket *packet); //send sensor packet to jetson
int init_connection(struct sockaddr_in *dest_addr, const char *ip, uint16_t port); //open a connection with the jetson

#ifdef __cplusplus
static_assert(sizeof(SensorPacket) == PACKET_SIZE, "SensorPacket size mismatch");
static_assert(sizeof(MotorPacket) == PACKET_SIZE, "MotorPacket size mismatch");
#else
_Static_assert(sizeof(SensorPacket) == PACKET_SIZE, "SensorPacket size mismatch");
_Static_assert(sizeof(MotorPacket) == PACKET_SIZE, "MotorPacket size mismatch");
#endif /* #ifdef __cplusplus */

#ifdef __cplusplus
} // extern "C"
#endif /* #ifdef __cplusplus */

#endif /* #ifndef CONNECTION_H */ 