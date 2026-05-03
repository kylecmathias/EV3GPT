import socket
import struct
import time

EV3_IP = "10.100.5.19"
PORT = 8888
CRC_INIT = 0x00

def crc8(data):
    crc = CRC_INIT
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80: crc = (crc << 1) ^ 0x07
            else: crc <<= 1
            crc &= 0xFF
    return crc

def send_motor_command(sock: socket.socket, speeds=[0, 50, 25, 100], stop_mode=0x01):
    header = 0xF9 
    duration = 0         
    
    payload = struct.pack(">B bbbb H B", header, *speeds, duration, stop_mode)
    packet = payload + struct.pack("B", crc8(payload))
    sock.sendto(packet, (EV3_IP, PORT))

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('', PORT)) 
    sock.setblocking(False)
    
    claw_open = True 
    
    print(f"Starting test. Sending to {EV3_IP}:{PORT}...")
    
    try:
        while True:
            motor_a_speed = 30 if claw_open else -30
            
            send_motor_command(sock, speeds=[motor_a_speed, 50, 25, 100])
            
            claw_open = not claw_open
            
            latest_data = None
            while True:
                try:
                    data, addr = sock.recvfrom(1024)
                    latest_data = data
                except BlockingIOError:
                    break 
            
            if latest_data and len(latest_data) == 9:
                unpacked = struct.unpack(">B BB h Bb B B", latest_data)
                print(f"Received [TS:{unpacked[6]:03}] Data: {unpacked} | Claw: {'Open' if not claw_open else 'Close'}")
            
            time.sleep(1.0) 
            
    except KeyboardInterrupt:
        print("\nStopping Test: Sending Emergency Stop...")
        try:
            for _ in range(3):
                send_motor_command(sock, speeds=[0, 0, 0, 0], stop_mode=0x03)
                time.sleep(0.05)
            print("Motors killed.")
        except Exception as e:
            print(f"Failed to send stop packet: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    main()