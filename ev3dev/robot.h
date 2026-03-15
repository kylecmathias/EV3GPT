#ifndef ROBOT_H
#define ROBOT_H

#include "config.h"
#include "connection.h"

#define MOTOR_BASE_STR "/sys/class/tacho-motor/motor"
#define SENSOR_BASE_STR "/sys/class/lego-sensor/sensor"
#define MAX_DEVICE_FOLDERS 10
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

enum class ColorID {
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
    inline const SensorConfig Color = {"COL-COLOR", 9};
    inline const SensorConfig Gyro = {"GYRO-RATE", 9};
    inline const SensorConfig Infrared = {"IR-SEEK", 7};
}

namespace MotorStops {
    inline const char* Brake = "brake";
    inline const char* Coast = "coast";
    inline const char* Hold  = "hold";
}

typedef uint8_t portmask;
typedef char smmask;

typedef struct {
    int sensors[MAX_PORT][SENSOR_DEPTH]; 
    int speed[MAX_PORT];   
    int command[MAX_PORT]; 
} PortFDs;

class Robot {
    private:
        PortFDs fds;
        portmask active; // A B C D 1 2 3 4, BIG ENDIAN
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

        smmask getMask(bool motor);
};

#endif /* #ifndef ROBOT_H */