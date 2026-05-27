import socket
import struct
import asyncio
from dataclasses import dataclass
from fastcrc import crc8

#EV3->Jetson: [0: Header][1: US][2: Color][3: Gyro_H][4: Gyro_L][5: IR_Prox][6: IR_Angle][7: TS][8: CRC]
#Jetson->EV3: [0: Header][1: Motor_A][2: Motor_B][3: Motor_C][4: Motor_D][5: Duration_H][6: Duration_L][7: STOP][8: CRC]

SENSOR_PACKET_FORMAT = "!BBBhBbBB" #EV3->Jetson
MOTOR_PACKET_FORMAT = "!BbbbbHB" #Jetson->EV3

LISTEN_PORT = 8888
GYRO_FP = 2 ** 4 #12/4 2-byte split
GYRO_IDX = 3
PACKET_LEN = 9
CRC_INIT = 0x00

@dataclass
class MotorCommand:
    """Structure for outgoing motor commands"""
    header: int
    motor_a: int
    motor_b: int
    motor_c: int
    motor_d: int
    duration: int
    stop: int

    def to_tuple(self):
        """Helper to get the tuple format for send()"""
        return (self.header, self.motor_a, self.motor_b, self.motor_c, self.motor_d, self.duration, self.stop)
    
@dataclass
class SensorData:
    """Structure for incoming EV3 sensor data"""
    header: int
    ultrasonic: int
    color: int
    gyro: float
    ir_prox: int
    ir_angle: int
    timestamp: int

class Connection:
    """
    Handles connections between Jetson and EV3 including receiving sensor data and sending motor commands.
    """
    _instance = None
    _initialized = False

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = super(Connection, cls).__new__(cls)
        return cls._instance
    
    def __init__(self, sock: int | None=None):
        if self._initialized: return
        
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.bind(("", sock if sock is not None else LISTEN_PORT))
        self._sock.setblocking(False)
        self._loop = asyncio.get_event_loop()
        self._addr = None
        self._latest_data = asyncio.Queue(maxsize=1)

        self._initialized = True

    async def start_listener(self) -> None:
        """
        Start receiver for incoming sensor data packets.
        """
        while True:
            while True:
                try:
                    data, addr = self._sock.recvfrom(PACKET_LEN)
                    if len(data) == PACKET_LEN and crc8.cdma2000(data[:-1], CRC_INIT) == data[-1]: #stripped field is crc8
                        unpacked = struct.unpack(SENSOR_PACKET_FORMAT, data)
                        unpacked = unpacked[:-1]

                        sensor_data = SensorData(*unpacked)
                        sensor_data.gyro /= GYRO_FP

                        if self._latest_data.empty():
                            await self._latest_data.put(sensor_data)
                        else:
                            try:
                                self._latest_data.get_nowait()
                                self._latest_data.put_nowait(sensor_data)
                            except asyncio.QueueEmpty:
                                pass
                        self._addr = addr
                except BlockingIOError:
                    break
            await asyncio.sleep(0.01)   

    async def send(self, command: MotorCommand) -> bool:
        """Send motor command packet to EV3."""
        if not self._addr: 
            return False
        
        packet = struct.pack(MOTOR_PACKET_FORMAT, *command.to_tuple())

        crc = crc8.cdma2000(packet, 0x00)
        packet += struct.pack("B", crc)

        try:
            await self._loop.sock_sendto(self._sock, packet, self._addr)
        except OSError as e:
            print(f"Unable to send packet to ev3: {e}")
            return False
        
        return True
    
    async def sensor_data(self) -> SensorData | None:
        """Get next available packet if exists"""
        if self._latest_data.empty():
            return None
        else:
            try:
                return self._latest_data.get_nowait()
            except asyncio.QueueEmpty:
                return 
            
    def addr(self) -> str:
        if not self._addr:
            raise RuntimeError("EV3 connection not initialized, address DNE")
        return self._addr