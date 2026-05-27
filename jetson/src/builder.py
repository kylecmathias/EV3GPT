import json

from commands import CommandBlockBase, CommandBlock, CommandChain, CommandBlockResult
from connection import MotorCommand, Connection

MOTOR_IDX = {"A": 0, "B": 1, "C": 2, "D": 3}
STOP_BRAKE = 0x01
STOP_COAST = 0x00
SYNC_BIT = 0x01
PORT_SHIFT = 3

FILLER = r"(ok|uh|um|like)"

ACTIONS = {
    "move": r"(go|move|drive|walk|run|travel)",
    "attack": r"(shoot|fire|attack|hit|launch|target)",
    "stop": r"(stop|halt|stay|freeze)",
    "interact": r"(grab|pick|touch|use|interact)",
    "follow": r"(follow|come)",
}

SPECIAL = {
    "edit_chain": [r"(append|modify|edit|change)", r"(chain)"],
}

ACTION_DEFAULTS = {
    "move": {"power": 50, "duration": 1000, "direction": "forward"},
    "rotate": {"power": 50, "degrees": 180,   "direction": "clockwise"},
    "attack": {"power": 100, "rotations": 1},
    "stop": {},
    "interact": {"power": 60, "rotations": 1},
    "follow": {"power": 50},
}

MOTOR_MAPPINGS = { #A: Claw, B: Right motor, C: Left motor, D: Shooter
    "move": {
        "forward":  {"B": 1, "C": 1},  
        "backward": {"B": -1, "C": -1},
    },
    "rotate": {
        "clockwise":        {"B": -1, "C": 1},
        "counterclockwise": {"B": 1, "C": -1},
    },
    "attack": {"D": 1},   
    "interact": {"A": 1},
    "stop": {"A": 0, "B": 0, "C": 0, "D": 0},
    "follow": {"B": 1, "C": 1},
}

async def move(**kwargs) -> tuple[CommandBlockResult, str]:
    return CommandBlockResult.SUCCESS, ""

def chain_builder(normalized_json: str, loop: bool = False, clear: bool = False) -> CommandChain | None:
    """
    Build a CommandChain from the LLM-normalized JSON string.
    
    Expected format: [[{action, ...params}, ...parallel], ...sequential]
    """
    try:
        hierarchy: list[list[dict]] = json.loads(normalized_json)
    except json.JSONDecodeError as e:
        print(f"chain_builder: failed to parse JSON: {e}")
        return None

    if not isinstance(hierarchy, list) or len(hierarchy) == 0:
        return None

    chain = CommandChain(loop=loop, clear=clear)

    for i, parallel_group in enumerate(hierarchy):
        if not isinstance(parallel_group, list) or len(parallel_group) == 0:
            continue

        is_last = (i == len(hierarchy) - 1)
        sync = not is_last

        if len(parallel_group) == 1:
            block = block_builder(parallel_group[0], sync=sync)
            if block:
                chain.append(block)
        else:
            #TODO: Make the parser use parallel blocks when parallel_group > 1
            for action_obj in parallel_group:
                block = block_builder(action_obj, sync=True)
                if block:
                    chain.append(block)

    if chain._nodes == 0:
        return None

    return chain

def block_builder(actions: dict, sync: bool) -> CommandBlockBase | None:
    """Build command block from parsed object"""
    action = actions.get("action", "").lower()
    if action not in ACTION_DEFAULTS:
        return None
    
    parameters = {k: v for k, v in actions.items() if k != "action"}
    command = _build_motor_command(action, parameters)
    if command is None:
        return None
    
    if sync:
        command.header |= SYNC_BIT

    #TODO: Make parallel blocks trigger async parallel callbacks gathered
    async def execute_commandblock(**kwargs) -> tuple[CommandBlockResult, str]:
        connection = Connection()
        success = await connection.send(kwargs["command"])
        return (CommandBlockResult.SUCCESS, "") if success else (CommandBlockResult.SEND_FAIL, "")
    
    block = CommandBlock(
        name=action,
        action=action,
        parameters={"command": command},
        stoppable=True
    )
    block._callback = execute_commandblock
    return block
    

def _build_motor_command(action: str, parameters: dict) -> MotorCommand | None:
    """Turn parsed action and parameters into motorcommand"""
    if action not in ACTION_DEFAULTS: 
        return None
    
    merged = {**ACTION_DEFAULTS[action], **parameters}

    mapping = MOTOR_MAPPINGS.get(action)
    if mapping is None:
        return None
    
    direction = merged.get("direction")
    if direction and isinstance(list(mapping.values())[0], dict):
        motor_map = mapping.get(direction)
        if motor_map is None:
            return None
    
    else:
        motor_map = mapping
    
    power = int(merged.get("power", 50))
    duration = int(merged.get("duration", 1000))

    speeds = [0, 0, 0, 0]
    port_mask = 0x00

    for motor, sign in motor_map.items():
        idx = MOTOR_IDX[motor]
        speeds[idx] = power * sign

        if sign != 0:
            port_mask |= (0x08 >> idx)

    stop = STOP_BRAKE if action == "stop" else STOP_COAST

    header = (port_mask << PORT_SHIFT) & 0x78

    return MotorCommand(
        header=header,
        motor_a=speeds[0],
        motor_b=speeds[1],
        motor_c=speeds[2],
        motor_d=speeds[3],
        duration=duration,
        stop=stop
    )