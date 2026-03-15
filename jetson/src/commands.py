from __future__ import annotations
from abc import ABC, abstractmethod
import asyncio
from enum import Enum
import janus

class CommandChainState(Enum):
    IDLE = 0
    RUNNING = 1
    PAUSED = 2
    FINISHED = 3

class CommandBlockBase(ABC):
    """
    Sequential command block base class
    """
    def __init__(self, **kwargs):
        self._name = kwargs.get("name", "command") #name of the 
        self._action = kwargs.get("action", "")
        self._subject = kwargs.get("subject", "")
        self._conditions = kwargs.get("conditions", [])
        self._parameters = kwargs.get("parameters", [])
        self._delay = kwargs.get("delay", 0)
        self._stoppable = kwargs.get("stoppable", False)
        self._next = None
        self._stop = None
    
    def chain(self, other: "CommandBlockBase") -> None:
        self._next = other

    @abstractmethod
    async def execute(self) -> None:
        pass

    @abstractmethod
    def end(self) -> None:
        pass

class CommandRoot(CommandBlockBase):
    """
    Root command block class does absolutely nothing other than point to the first command block
    """
    def __init__(self):
        super().__init__(name="root")

    async def execute(self) -> None:
        raise NotImplementedError("Root block does not execute")
    
    def end(self) -> None:
        raise NotImplementedError("Root block does not end")
    
class CommandBlock(CommandBlockBase):
    """
    Command block class that executes a command in a chain
    """
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    async def execute(self) -> None:
        pass

class CommandChain:
    """
    Structure holding and tracking the command block chain
    """
    def __init__(self, loop=False, clear=False):
        self._root = CommandRoot()
        self._last = self._root
        self._nodes = 0
        self._loop = loop
        self._state = CommandChainState.IDLE
        self._clear = clear

    def chain(self, block: CommandBlockBase):
        self._last.chain(block)
        self._last = block
        self._nodes += 1

    async def run(self) -> None:
        if self._nodes <= 0:
            return
        
        self._state = CommandChainState.RUNNING
        current = self._root._next

        while current is not None and self._state == CommandChainState.RUNNING:
            await current.execute()
            if current._delay > 0:
                await asyncio.sleep(current._delay)

            current = current._next

            if current is None and self._loop:
                current = self._root._next

class CommandQueue(janus.Queue):
    """
    Structure that holds multiple chains in a queue in order of creation
    """
    def __init__(self):
        super().__init__(maxsize=8)

    def clear_all(self) -> None:
        while not self.sync_q.empty():
            self.sync_q.get_nowait()