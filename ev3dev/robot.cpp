#include "robot.h"

Robot::Robot() {
    for (int i = 0; i < MAX_PORT; i++) {
        fds.speed[i] = -1;
        fds.command[i] = -1;
        fds.stop[i] = -1;
        for (int j = 0; j < SENSOR_DEPTH; j++) {
           fds.sensors[i][j] = -1; 
        }
    }
    active = 0x00U;
    running = 0x00U;
    stops = 0x00U;
    matchPorts();
}

Robot::~Robot() {
    for (int i = 0; i < MAX_PORT; i++) {
        if (fds.command[i] != -1) {
            if (dprintf(fds.command[i], STOP_STR) < 0) {}
            close(fds.command[i]);
        }
        if (fds.stop[i] != -1) {
            close(fds.stop[i]);
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
            std::cout << "DEBUG: Found Motor Folder [" << i << "] with Address: [" << addr << "]" << std::endl;
            int idx = -1;

            if (addr.find("outA") != std::string::npos) idx = MOTOR_A;
            if (addr.find("outB") != std::string::npos) idx = MOTOR_B;
            if (addr.find("outC") != std::string::npos) idx = MOTOR_C;
            if (addr.find("outD") != std::string::npos) idx = MOTOR_D;

            if (idx != -1) {
                fds.speed[idx] = open((path + "/duty_cycle_sp").c_str(), O_WRONLY | O_CLOEXEC);
                fds.command[idx] = open((path + "/command").c_str(), O_WRONLY | O_CLOEXEC);
                fds.stop[idx] = open((path + "/stop_action").c_str(), O_WRONLY | O_CLOEXEC);
                
                if (fds.speed[idx] < 0 || fds.command[idx] < 0) {
                    std::cout << "Error opening either speed: " << fds.speed[idx] << " or command: " << fds.command[idx] << " fds" << std::endl;
                    continue;
                }

                if (dprintf(fds.speed[idx], "0\n") < 0) {}
                if (dprintf(fds.command[idx], RUN_DIRECT_STR) < 0) {}
                SET_PORT_MASK(active, idx);
            }
        }
    }

    for (int i = 0; i < MAX_DEVICE_FOLDERS; i++) {
        std::string path = SENSOR_BASE_STR + std::to_string(i);
        std::ifstream addrFile(path + "/address");
        if (addrFile.is_open()) {
            std::string addr;
            addrFile >> addr;
            std::cout << "DEBUG: Found Sensor Folder [" << i << "] with Address: [" << addr << "]" << std::endl;
            int idx = -1;

            if (addr.find("in1") != std::string::npos) idx = SENSOR_1;
            if (addr.find("in2") != std::string::npos) idx = SENSOR_2;
            if (addr.find("in3") != std::string::npos) idx = SENSOR_3;
            if (addr.find("in4") != std::string::npos) idx = SENSOR_4;

            if (idx != -1) {
                sensor_paths[idx] = path; 
                fds.sensors[idx][0] = open((path + "/value0").c_str(), O_RDONLY | O_CLOEXEC);
                
                if (fds.sensors[idx][0] < 0) {
                    std::cout << "Cant open sensor fd: " << fds.sensors[idx][0] << std::endl;
                    continue;
                }

                SET_PORT_MASK(active, idx + SENSOR_OFFSET);

                std::ifstream driverFile(path + "/driver_name");
                std::string driver;
                driverFile >> driver;
                if (driver == "lego-ev3-ir" || driver == "lego-ev3-gyro") {
                    fds.sensors[idx][1] = open((path + "/value1").c_str(), O_RDONLY | O_CLOEXEC);
                }
                else {
                    fds.sensors[idx][1] = -1; 
                }
                
            }
        }
    }
}

void Robot::initSensor(int portIndex, SensorConfig config) {
    if (fds.sensors[portIndex][0] == -1) return;

    std::string path = sensor_paths[portIndex] + "/mode";
    int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    
    if (fd != -1) {
        if (write(fd, config.mode, config.len) < 0) {}
        if (write(fd, "\n", 1) < 0) {}
        usleep(200000);
        close(fd);
    }
}

int Robot::readSensor(int portIndex, int valueIndex) {
    if (portIndex < 0 || portIndex >= MAX_PORT || valueIndex < 0 || valueIndex >= SENSOR_DEPTH || fds.sensors[portIndex][valueIndex] == -1) 
        return -1;

    char buf[SENSOR_BUF_SIZE];

    lseek(fds.sensors[portIndex][valueIndex], 0, SEEK_SET); 
    ssize_t bytesRead = read(fds.sensors[portIndex][valueIndex], buf, sizeof(buf) - 1);

    if (bytesRead > 0) {
        buf[bytesRead] = '\0';
        //std::cout << "Read from sensor " << (portIndex == 0 ? "Color" : portIndex == 1 ? "Gyro" : portIndex == 2 ? "Ultrasonic" : "Infrared") << ": " << buf << "\n";
        return atoi(buf);
    }
    std::cout << "Error reading sensor " << (portIndex == 0 ? "Color" : portIndex == 1 ? "Gyro" : portIndex == 2 ? "Ultrasonic" : "Infrared") << " at value:" << valueIndex << "\n";
    return -1;
}

void Robot::writeMotor(int portIndex, int speed) {
    if (portIndex < 0 || portIndex >= MAX_PORT || fds.speed[portIndex] == -1) 
        return;

    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;

    lseek(fds.speed[portIndex], 0, SEEK_SET);
    if (dprintf(fds.speed[portIndex], "%d\n", speed) < 0) {}    
    
    SET_PORT_MASK(running, portIndex);
}
    
void Robot::executeMotorCommands(MotorCommand command) {
    current = command;
    int fd;

    if (current.ports & MOTOR_A_MASK) {
        //std::cout << "Running motor A" << std::endl;
        writeMotor(MOTOR_A, current.speeds[MOTOR_A]);
        if (GET_STOP_MASK(stops, MOTOR_A) != command.stop) { 
            fd = fds.stop[MOTOR_A];
            if (fd != -1 && dprintf(fd, GET_STOP_STR(command.stop)) >= 0) SET_STOP_MASK(stops, MOTOR_A, command.stop);
        }
    }
    if (current.ports & MOTOR_B_MASK) {
        //std::cout << "Running motor B" << std::endl;
        writeMotor(MOTOR_B, current.speeds[MOTOR_B]);
        if (GET_STOP_MASK(stops, MOTOR_B) != command.stop) { 
            fd = fds.stop[MOTOR_B];
            if (fd != -1 && dprintf(fd, GET_STOP_STR(command.stop)) >= 0) SET_STOP_MASK(stops, MOTOR_B, command.stop);
        }
    }
    if (current.ports & MOTOR_C_MASK) {
        //std::cout << "Running motor C" << std::endl;
        writeMotor(MOTOR_C, current.speeds[MOTOR_C]);
        if (GET_STOP_MASK(stops, MOTOR_C) != command.stop) { 
            fd = fds.stop[MOTOR_C];
            if (fd != -1 && dprintf(fd, GET_STOP_STR(command.stop)) >= 0) SET_STOP_MASK(stops, MOTOR_C, command.stop);
        }
    }
    if (current.ports & MOTOR_D_MASK) { 
        //std::cout << "Running motor D" << std::endl;
        writeMotor(MOTOR_D, current.speeds[MOTOR_D]);
        if (GET_STOP_MASK(stops, MOTOR_D) != command.stop) { 
            fd = fds.stop[MOTOR_D];
            if (fd != -1 && dprintf(fd, GET_STOP_STR(command.stop)) >= 0) SET_STOP_MASK(stops, MOTOR_D, command.stop);
        }
    }
    if (!current.sync) {
        struct timespec run_time;
        run_time.tv_sec = current.duration / 1000 ;
        run_time.tv_nsec = (current.duration % 1000) * 1000000L;
        nanosleep(&run_time, NULL);
    }
}

void Robot::resetGyro() {
    if (fds.sensors[GYRO][0] == -1) return;

    std::cout << "Calibrating Gyro, Keep the robot still." << std::endl;

    initSensor(GYRO, SensorModes::GyroReset);
    
    usleep(500000); 

    initSensor(GYRO, SensorModes::Gyro);
    
    usleep(500000);
    
    std::cout << "Gyro Zeroed." << std::endl;
}

smmask Robot::getMask(bool motor) {
    if (motor) return (smmask) ((active & MOTOR_MASK) >> MAX_PORT);
    else return (smmask) (active & SENSOR_MASK);
}
