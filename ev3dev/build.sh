EV3_IP=ev3gpt.local

arm-linux-gnueabi-g++ -march=armv5te -O3 -fexceptions -fno-rtti -fno-threadsafe-statics -ffunction-sections -fdata-sections ev3.cpp robot.cpp tasks.cpp connection.c audio.cpp -o ev3 -Wl,--gc-sections -lpthread -lasound -s

scp -i ~/ev3key ev3 robot@$EV3_IP:/home/robot/

ssh -i ~/ev3key robot@$EV3_IP "chmod +x /home/robot/ev3 && sudo setcap cap_sys_nice=ep /home/robot/ev3 && exit"