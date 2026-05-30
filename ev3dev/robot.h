#ifndef ROBOT_H
#define ROBOT_H

#include "config.h"
#include "connection.h"

#define MOTOR_BASE_STR "/sys/class/tacho-motor/motor"
#define SENSOR_BASE_STR "/sys/class/lego-sensor/sensor"
#define MAX_DEVICE_FOLDERS 30
#define MAX_PORT 4
#define SENSOR_DEPTH 2
#define SENSOR 0
#define MOTOR 1
#define MOTOR_A 0 //right medium motor (claw)
#define MOTOR_B 1 //right tread large motor 
#define MOTOR_C 2 //left tread large motor 
#define MOTOR_D 3 //left medium motor (shoot)
#define SENSOR_1 0 //color sensor
#define SENSOR_2 1 //gyro sensor
#define SENSOR_3 2 //ultrasonic sensor
#define SENSOR_4 3 //infrared sensor
#define GYRO_MODE 0x00U //fixed point angle 12/4

#define MEDIUM_RIGHT MOTOR_A
#define MEDIUM_LEFT MOTOR_D
#define LARGE_RIGHT MOTOR_B
#define LARGE_LEFT MOTOR_C
#define COLOR SENSOR_1
#define GYRO SENSOR_2
#define ULTRASONIC SENSOR_3
#define INFRARED SENSOR_4

#define IR_PROX 1
#define IR_ANGLE 0

#define SENSOR_BUF_SIZE 32
#define SENSOR_OFFSET 4

#define MOTOR_A_MASK 0x08U //0b0000 1000
#define MOTOR_B_MASK 0x04U //0b0000 0100
#define MOTOR_C_MASK 0x02U //0b0000 0010
#define MOTOR_D_MASK 0x01U //0b0000 0001

#define TREADS 0x06U //0b0000 0110
#define REVERSE_TIME 0x05DCU //1500
#define MAX_SPEED_REVERSE (int8_t)0x80 //-128

#define SENSOR_MASK 0x0FU
#define MOTOR_MASK 0xF0U

#define SET_PORT_MASK(mask, port) (mask |= (128U >> port)) //port should be 0, 1, 2, or 3
#define GET_PORT_MASK(mask, port) (mask & (128U >> port))
#define CLEAR_PORT_MASK(mask, port) (mask &= ~(128U >> port))
#define GET_STOP_MASK(mask, port) (((mask & (192U >> (2 * port))) >> (6 - (2 * port))) & 0x03) //0b0000 0011
#define CLEAR_STOP_MASK(mask, port) (mask &= ~(192U >> (2 * port)))
#define SET_STOP_MASK(mask, port, stop) (mask = ((mask & ~(192U >> (2 * port))) | ((stop << 6) >> (2 *port)))) // 0b1100 0000 >> AA BB CC DD
#define GET_STOP_STR(stop) (stop == 0x02 ? "hold\n" : (stop == 0x01 ? "brake\n" : "coast\n"))

#define STOP_COMMAND {0x00, false, false, 0x0F, {0x00, 0x00, 0x00, 0x00}, 0x00C8, 0x01} //0x00, nosync, nointerrupt, all ports, 0 speed, 0 duration, brake
#define REVERSE_COMMAND {0x00, false, false, TREADS, {0x00, MAX_SPEED_REVERSE, MAX_SPEED_REVERSE, 0x00}, REVERSE_TIME, 0x01} //0x00, nosync, nointerrupt, ports B and C, -128 speed, 1500ms, brake

#define GROUNDED_THRESHOLD 5

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

constexpr const char* RUN_FOREVER_STR = "run-forever\n";
constexpr const char* RUN_DIRECT_STR = "run-direct\n";
constexpr const char* STOP_STR = "stop\n";

typedef uint8_t portmask;
typedef char smmask;

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
    inline const char* Brake = "brake";
    inline const char* Coast = "coast";
    inline const char* Hold  = "hold";
}

typedef struct {
    int sensors[MAX_PORT][SENSOR_DEPTH]; 
    int speed[MAX_PORT];   
    int command[MAX_PORT]; 
    int stop[MAX_PORT];
} PortFDs;

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

inline int asciitoint(const char *str) {
    if (str == NULL) {
        return 0;
    }

    int i = 0;
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

#endif /* #ifndef ROBOT_H */