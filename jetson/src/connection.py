import socket
import struct
import asyncio
from fastcrc import crc8
from typing import AsyncGenerator

#EV3->Jetson: [0: Header][1: US][2: Color][3: Gyro_H][4: Gyro_L][5: IR_Prox][6: IR_Angle][7: TS][8: CRC]
#Jetson->EV3: [0: Header][1: Motor_A][2: Motor_B][3: Motor_C][4: Motor_D][5: Duration_H][6: Duration_L][7: STOP][8: CRC]

SENSOR_PACKET_FORMAT = "!BBBhBbBB" #EV3->Jetson
MOTOR_PACKET_FORMAT = "!BbbbbHB" #Jetson->EV3

LISTEN_PORT = 8888
GYRO_FP = 2 ** 4 #12/4 2-byte split

class Connection:
    """
    Handles connections between Jetson and EV3 including receiving sensor data and sending motor commands.
    """
    def __init__(self, sock: int | None=None):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.bind(("", sock if sock is not None else LISTEN_PORT))
        self._sock.setblocking(False)
        self._loop = asyncio.get_event_loop()
        self._addr = None
        self._latest_data = None

    async def start_listener(self) -> None:
        """
        Start receiver for incoming sensor data packets.
        """
        while True:
            while True:
                try:
                    data, addr = self._sock.recvfrom(9)
                    if len(data) == 9 and crc8.cdma2000(data[:-1], 0x00) == data[-1]:
                        unpacked_data = struct.unpack(SENSOR_PACKET_FORMAT, data)
                        self._latest_data = unpacked_data[:3] + (unpacked_data[3] / GYRO_FP,) + unpacked_data[4:8]
                        self._addr = addr
                except BlockingIOError:
                    break
            await asyncio.sleep(0.01)
            

    async def send(self, data: tuple[int, int, int, int, int, int, int]):
        """
        Send motor command packet to EV3.
        """
        packet = struct.pack(MOTOR_PACKET_FORMAT, *data)

        crc = crc8.cdma2000(packet, 0x00)
        packet += struct.pack("B", crc)

        if self._addr is not None:
            await self._loop.sock_sendto(self._sock, packet, self._addr)