arm-linux-gnueabi-g++ -static -march=armv5te -O3 ev3.cpp robot.cpp tasks.cpp connection.c -o ev3 -lpthread

scp -i ~/ev3key ev3 robot@10.100.5.19:/home/robot/

ssh -i ~/ev3key robot@10.100.5.19 "chmod +x /home/robot/ev3 | exit"