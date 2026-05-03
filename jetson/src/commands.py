from __future__ import annotations
import asyncio
import janus
from enum import Enum
from abc import ABC, abstractmethod

class CommandChainState(Enum):
    """Enum for state of command chain\n
    0: Idle\n
    1: Running\n
    2: Paused\n
    3: Finished
    """
    IDLE = 0
    RUNNING = 1
    PAUSED = 2
    FINISHED = 3

class CommandBlockBase(ABC):
    """
    Abstract command block class to be overridden
    """
    def __init__(self, **kwargs):
        self._name = kwargs.get("name", "command") #name of the command
        self._action = kwargs.get("action", "")
        self._subject = kwargs.get("subject", "")
        self._conditions = kwargs.get("conditions", [])
        self._parameters = kwargs.get("parameters", [])
        self._delay = kwargs.get("delay", 0)
        self._stoppable = kwargs.get("stoppable", False)
        self._next = None
        self._stop = None

    def next(self) -> int | None:
        """Returns index of next command block, or None if current command block is last in chain"""
        return self._next

    @abstractmethod
    async def execute(self) -> None:
        raise NotImplementedError("Cannot call execute() on CommandBlockBase class")

    @abstractmethod
    def end(self) -> None:
        raise NotImplementedError("Cannot call end() on CommandBlockBase class")
    
class CommandBlock(CommandBlockBase):
    """
    Command block class that executes a command in a chain
    """
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    async def execute(self) -> None:
        pass

    def end(self) -> None:
        pass

class CommandChain:
    """
    Structure holding and tracking the command block chain
    """
    def __init__(self, loop=False, clear=False):
        self._chain: list[CommandBlockBase] = []
        self._nodes = 0
        self._loop = loop
        self._state = CommandChainState.IDLE
        self._clear = clear #clear on complete if not looping

    def append(self, block: CommandBlockBase):
        self._chain.append(block)
        self._nodes += 1

    async def run(self) -> None:
        if self._nodes <= 0:
            return

class CommandQueue(janus.Queue):
    """
    Structure that holds multiple chains in a queue in order of creation
    """
    def __init__(self):
        super().__init__(maxsize=8)

    def clear_all(self) -> None:
        while not self.sync_q.empty():
            self.sync_q.get_nowait()