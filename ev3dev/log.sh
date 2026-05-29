EV3_IP=ev3gpt.local

ssh -i ./ev3key robot@$EV3_IP "cat /home/robot/ev3.err.log"