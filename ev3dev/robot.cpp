#include "robot.h"

Robot::Robot() {
    for (int i = 0; i < MAX_PORT; i++) {
        fds.speed[i] = -1;
        fds.command[i] = -1;
        for (int j = 0; j < SENSOR_DEPTH; j++) {
           fds.sensors[i][j] = -1; 
        }
    }
    active = 0;
    matchPorts();
}
Robot::~Robot() {
    for (int i = 0; i < MAX_PORT; i++) {
        if (fds.command[i] != -1) {
            if (write(fds.command[i], "stop", 4) < 0) {}
            close(fds.command[i]);
        }
        if (fds.speed[i] != -1) close(fds.speed[i]);
        for (int j = 0; j < SENSOR_DEPTH; j++) {
            if (fds.sensors[i][j] != -1) close(fds.sensors[i][j]);
        }
    }
}

void Robot::matchPorts() {
    for (int i = 0; i < MAX_DEVICE_FOLDERS; i++) {
        std::string path = MOTOR_BASE_STR + std::to_string(i);
        std::ifstream addrFile(path + "/address");
        if (addrFile.is_open()) {
            std::string addr;
            addrFile >> addr;
            int idx = -1;

            if (addr == "outA") idx = MOTOR_A;
            else if (addr == "outB") idx = MOTOR_B;
            else if (addr == "outC") idx = MOTOR_C;
            else if (addr == "outD") idx = MOTOR_D;

            if (idx != -1) {
                fds.speed[idx] = open((path + "/speed_sp").c_str(), O_WRONLY);
                fds.command[idx] = open((path + "/command").c_str(), O_WRONLY);
                if (write(fds.command[idx], "run-forever", 11) < 0) {};
                active |= (128U >> idx);
            }
        }
    }

    for (int i = 0; i < MAX_DEVICE_FOLDERS; i++) {
        std::string path = SENSOR_BASE_STR + std::to_string(i);
        std::ifstream addrFile(path + "/address");
        if (addrFile.is_open()) {
            std::string addr;
            addrFile >> addr;
            int idx = -1;

            if (addr == "in1") idx = SENSOR_1;
            else if (addr == "in2") idx = SENSOR_2;
            else if (addr == "in3") idx = SENSOR_3;
            else if (addr == "in4") idx = SENSOR_4;

            if (idx != -1) {
                sensor_paths[idx] = path; 
                fds.sensors[idx][0] = open((path + "/value0").c_str(), O_RDONLY);
                active |= (128U >> (idx + 4));

                std::ifstream driverFile(path + "/driver_name");
                std::string driver;
                driverFile >> driver;
                if (driver == "lego-ev3-ir") {
                    fds.sensors[idx][1] = open((path + "/value1").c_str(), O_RDONLY);
                } else {
                    fds.sensors[idx][1] = -1; 
                }
            }
        }
    }
}

void Robot::initSensor(int portIndex, SensorConfig config) {
    if (fds.sensors[portIndex][0] == -1) return;

    std::string path = sensor_paths[portIndex] + "/mode";
    int fd = open(path.c_str(), O_WRONLY);
    
    if (fd != -1) {
        if (write(fd, config.mode, config.len) < 0) {}
        close(fd);
    }
}

int Robot::readSensor(int portIndex, int valueIndex) {
    if (portIndex < 0 || portIndex >= MAX_PORT || fds.sensors[portIndex][valueIndex] == -1) 
        return -1;

    char buf[16];
    ssize_t bytesRead = pread(fds.sensors[portIndex][valueIndex], buf, sizeof(buf) - 1, 0);
    if (bytesRead > 0) {
        buf[bytesRead] = '\0';
        return atoi(buf);
    }
    return -1;
}

void Robot::writeMotor(int portIndex, int speed) {
    if (portIndex < 0 || portIndex >= MAX_PORT || fds.speed[portIndex] == -1) 
        return;

    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;
    dprintf(fds.speed[portIndex], "%d", speed);
}

void Robot::executeMotorCommands(MotorCommand command) {
    current = command;
    if (current.ports & MOTOR_A_MASK) {
        writeMotor(MOTOR_A, current.speeds[MOTOR_A]);
    }
    if (current.ports & MOTOR_B_MASK) {
        writeMotor(MOTOR_B, current.speeds[MOTOR_B]);
    }
    if (current.ports & MOTOR_C_MASK) {
        writeMotor(MOTOR_C, current.speeds[MOTOR_C]);
    }
    if (current.ports & MOTOR_D_MASK) { 
        writeMotor(MOTOR_D, current.speeds[MOTOR_D]);
    }
    if (!current.sync) {
        struct timespec run_time;
        run_time.tv_sec = current.duration / 1000 ;
        run_time.tv_nsec = (current.duration % 1000) * 1000000L;
        nanosleep(&run_time, NULL);
    }
}

smmask Robot::getMask(bool motor) {
    if (motor) return (smmask) ((active & 0xF0) >> MAX_PORT);
    else return (smmask) (active & 0x0F);
}
