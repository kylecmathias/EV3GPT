import asyncio
from src.connection import Connection
from src.controller import Controller

#TODO: Delete this function later
async def debug_loop():
    """Print sensor state every second to verify connection"""
    controller = Controller()
    while True:
        state = controller.state
        if state.connected:
            print(f"US: {state.ultrasonic}cm | Color: {state.color} | Gyro: {state.gyro:.1f}° | IR: {state.ir_prox} @ {state.ir_angle}° | TS: {state.timestamp}")
        else:
            print("[main] Waiting for EV3...")
        await asyncio.sleep(1.0)

async def main():
    """Main entry point to program"""
    connection = Connection()
    controller = Controller()
    
    await asyncio.gather(
        connection.start_listener(),
        controller.update_loop(),
        debug_loop()
    )

if __name__ == "__main__":
    asyncio.run(main())
    