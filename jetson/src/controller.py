import asyncio
import time

from dataclasses import dataclass, field
from connection import Connection, MotorCommand, SensorData
from commands import CommandQueue

IDLE_POLL_INTERVAL = 0.05
STALE_THRESHOLD = 2.0 #seconds before packet is stale

OBSTACLE_THRESHOLD_CM = 20

STOP_COMMAND = MotorCommand(
    header=0x78,
    motor_a=0,
    motor_b=0,
    motor_c=0,
    motor_d=0,
    duration=0,
    stop=0x00
)

@dataclass
class RobotState:
    """Current state of EV3 sensor readings"""
    ultrasonic: int = -1 #distance in cm (0-255)
    color: int = -1 #reflected light intensity (0-100)
    gyro: float = 0.0 #angle in degrees (-25-25)
    ir_prox: int = -1 #infrared proximity (0-100)
    ir_angle: int = 0 #infrared angle (-25-25)
    timestamp: int = 0 #EV3 rolling timestamp byte
    last_updated: float = 0.0 #time of last received packet
    connected: bool = False

    def is_stale(self) -> bool:
        """Whether the last received packet is older than STALE_THRESHOLD"""
        return (time.monotonic() - self.last_updated) > STALE_THRESHOLD

    def update(self, data: SensorData) -> None:
        self.ultrasonic = data.ultrasonic
        self.color = data.color
        self.gyro = data.gyro
        self.ir_prox = data.ir_prox
        self.ir_angle = data.ir_angle
        self.timestamp = data.timestamp
        self.last_updated = time.monotonic()
        self.connected = True

class Controller:
    """Class for robot state management and autonomous behavior.
    Consumes sensor packets from Connection, maintains RobotState,
    and handles idle/reactive behaviors when no command is queued"""

    _instance = None
    _initialized = False
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(Controller, cls).__new__(cls)
        return cls._instance

    def __init__(self):
        if self._initialized: return

        self._state = RobotState()
        self._connection = Connection()
        self._cmd_queue = CommandQueue()
        self._obstacle_stop_sent = False

        self._initialized = True

    @property
    def state(self) -> RobotState:
        """Read-only access to current robot state"""
        return self._state
    
    async def update_loop(self) -> None:
        """Main controller loop that runs continuously as an asyncio task.
        Pulls the latest sensor data and updates state, handles reactive
        behaviors (obstacle avoidance), and handles idle behavior when
        command queue is empty"""
        while True:
            await self._update_state()
            await self._reactive_behavior()
            await asyncio.sleep(IDLE_POLL_INTERVAL)

    async def _update_state(self) -> None:
        """Get latest sensor packet and update state"""
        data = await self._connection.sensor_data()
        if data is not None:
            self._state.update(data)
            self._obstacle_stop_sent = False
        elif self._state.connected and self._state.is_stale():
            self._state.connected = False
            print("EV3 lost connection")

    async def _reactive_behavior(self) -> None:
        """Reactive behavior based on current sensor state,
        only triggers when no chain currently running"""
        if self._cmd_queue.is_running(): return
        if not self._state.connected: return

        await self._obstacle_check()

    async def _obstacle_check(self):
        """Send stop if obstacle detected within threshold"""
        if self._state.ultrasonic == -1: return

        if self._state.ultrasonic < OBSTACLE_THRESHOLD_CM:
            if not self._obstacle_stop_sent:
                success = await self._connection.send(STOP_COMMAND)
                if success:
                    self._obstacle_stop_sent = True
                    print(f"Obstacle at {self._state.ultrasonic}cm, stop sent")

    async def send_idle(self) -> None:
        """"""
        pass #TODO: Add idling

    async def emergency_stop(self) -> None:
        """Immediately send stop and clear command queue"""
        self._cmd_queue.clear_all()
        await self._connection.send(STOP_COMMAND)
        print("Emergency stop")