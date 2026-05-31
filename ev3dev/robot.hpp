#pragma once

#include "config.hpp"
#include "connection.hpp"

#ifndef INT_MAX
#define INT_MAX (int32_t)0x7FFFFFFF
#endif /* #ifndef INT_MAX */
#ifndef INT_MIN
#define INT_MIN (-INT_MAX - 1)
#endif /* #ifndef INT_MIN */
#ifndef UINT_MAX
#define UINT_MAX (uint32_t)0xFFFFFFFF
#endif  /* #ifndef UINT_MAX */
#ifndef UINT_MIN
#define UINT_MIN (uint32_t)0x00000000
#endif /* #ifndef UINT_MIN */
#ifndef INT_MIN64
#define INT_MIN64 0x80000000LL
#endif /* #ifndef INT_MIN64 */

inline constexpr const char MOTOR_BASE_STR[] = "/sys/class/tacho-motor/motor";
inline constexpr const char SENSOR_BASE_STR[] = "/sys/class/lego-sensor/sensor";
inline constexpr size_t MAX_DEVICE_FOLDERS = 30;
inline constexpr size_t MAX_PORT = 4;
inline constexpr size_t SENSOR_DEPTH = 2;
inline constexpr bool SENSOR = 0;
inline constexpr bool MOTOR = 1;
inline constexpr uint8_t GYRO_MODE = 0x00; //fixed point angle 12/4

inline constexpr uint8_t MEDIUM_RIGHT = MOTOR_A;
inline constexpr uint8_t MEDIUM_LEFT = MOTOR_D;
inline constexpr uint8_t LARGE_RIGHT = MOTOR_B;
inline constexpr uint8_t LARGE_LEFT = MOTOR_C;
inline constexpr uint8_t COLOR = SENSOR_1;
inline constexpr uint8_t GYRO = SENSOR_2;
inline constexpr uint8_t ULTRASONIC = SENSOR_3;
inline constexpr uint8_t INFRARED = SENSOR_4;

inline constexpr bool IR_PROX = 1;
inline constexpr bool IR_ANGLE = 0;

inline constexpr size_t SENSOR_BUF_SIZE = 32;
inline constexpr int8_t SENSOR_OFFSET = 4;

inline constexpr uint8_t MOTOR_A_MASK = 0x08; //0b0000 1000
inline constexpr uint8_t MOTOR_B_MASK = 0x04; //0b0000 0100
inline constexpr uint8_t MOTOR_C_MASK = 0x02; //0b0000 0010
inline constexpr uint8_t MOTOR_D_MASK = 0x01; //0b0000 0001

inline constexpr uint8_t TREADS = 0x06; //0b0000 0110
inline constexpr uint16_t REVERSE_TIME = 1500; //1500
inline constexpr int8_t MAX_SPEED_REVERSE = -128; 

inline constexpr uint8_t SENSOR_MASK = 0x0F;
inline constexpr uint8_t MOTOR_MASK = 0xF0;

inline void SET_PORT_MASK(uint8_t& mask, uint8_t port) noexcept {mask |= (128U >> port);} //port should be 0, 1, 2, or 3
inline uint8_t GET_PORT_MASK(uint8_t mask, uint8_t port) noexcept {return mask & (128U >> port);}
inline void CLEAR_PORT_MASK(uint8_t& mask, uint8_t port) noexcept {mask &= ~(128U >> port);}
inline uint8_t GET_STOP_MASK(uint8_t mask, uint8_t port) noexcept {return ((mask & (192U >> (2 * port))) >> (6 - (2 * port))) & 0x03;} //0b0000 0011
inline void CLEAR_STOP_MASK(uint8_t& mask, uint8_t port) noexcept {mask &= ~(192U >> (2 * port));}
inline void SET_STOP_MASK(uint8_t& mask, uint8_t port, uint8_t stop) noexcept {mask = ((mask & ~(192U >> (2 * port))) | ((stop << 6) >> (2 *port)));} // 0b1100 0000 >> AA BB CC DD
inline const char* GET_STOP_STR(uint8_t stop) noexcept {return stop == 0x02 ? "hold\n" : (stop == 0x01 ? "brake\n" : "coast\n");}

inline constexpr MotorCommand STOP_COMMAND = {0x00, false, false, 0x0F, {0x00, 0x00, 0x00, 0x00}, 0x00C8, 0x01}; //0x00, nosync, nointerrupt, all ports, 0 speed, 0 duration, brake
inline constexpr MotorCommand REVERSE_COMMAND = {0x00, false, false, TREADS, {0x00, MAX_SPEED_REVERSE, MAX_SPEED_REVERSE, 0x00}, REVERSE_TIME, 0x01}; //0x00, nosync, nointerrupt, ports B and C, -128 speed, 1500ms, brake

inline constexpr uint8_t GROUNDED_THRESHOLD = 5;

inline constexpr const char RUN_FOREVER_STR[] = "run-forever\n";
inline constexpr const char RUN_DIRECT_STR[] = "run-direct\n";
inline constexpr const char STOP_STR[] = "stop\n";

using portmask = uint8_t;
using smmask = uint8_t;

inline int asciitoint(const char *str) {
    if (str == NULL) {
        return 0;
    }

    size_t i = 0;
    int sign = 1;
    int64_t result = 0;

    while (str[i] == ' ' || str[i] == '\t' || str[i] == '\n' || str[i] == '\r' || str[i] == '\v' || str[i] == '\f') {
        i++;
    }

    if (str[i] == '-') {
        sign = -1;
        i++;
    } 
    else if (str[i] == '+') {
        i++;
    }

    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');

        if ((result >= (int64_t)INT_MAX + 1 && sign == 1) || (result >= (int64_t)INT_MAX && sign == -1)) {
            return (sign == 1) ? INT_MAX : INT_MIN;
        }
        i++;
    }

    result *= sign;

    return (int)result;
}

enum class ColorID { //not needed since color sensor used for reflected light intensity
    None = 0,
    Black = 1,
    Blue = 2,
    Green = 3,
    Yellow = 4,
    Red = 5,
    White = 6,
    Brown = 7,
};

struct SensorConfig {
    const char* mode;
    size_t len;
};

namespace SensorModes {
    inline const SensorConfig Ultrasonic = {"US-DIST-CM", 10};
    inline const SensorConfig Color = {"COL-REFLECT", 11};
    inline const SensorConfig Gyro = {"GYRO-ANG", 9};
    inline const SensorConfig Infrared = {"IR-SEEK", 7};
    inline const SensorConfig GyroReset = {"GYRO-RATE", 9};
}

namespace MotorStops {
    inline const char Brake[] = "brake";
    inline const char Coast[] = "coast";
    inline const char Hold[]  = "hold";
}

struct PortFDs {
    int sensors[MAX_PORT][SENSOR_DEPTH]; 
    int speed[MAX_PORT];   
    int command[MAX_PORT]; 
    int stop[MAX_PORT];
};

class Robot {
    private:
        PortFDs fds;
        portmask active; //A B C D 1 2 3 4, BIG ENDIAN
        portmask running; //A B C D X X X X, BIG ENDIAN
        portmask stops; // A A B B C C D D, BIG ENDIAN
        std::string sensor_paths[MAX_PORT];
        MotorCommand current;
    private:
        void matchPorts();
    public:
        Robot();
        ~Robot(); 
        void initSensor(int portIndex, SensorConfig config);
        int readSensor(int portIndex, int valueIndex = 0); 
        void writeMotor(int portIndex, int speed);
        void executeMotorCommands(MotorCommand command);
        void resetGyro();
        smmask getMask(bool motor);
};
