from __future__ import annotations
import asyncio
import janus
from enum import Enum, auto
from abc import ABC, abstractmethod
from typing import Callable, Awaitable

class CommandChainState(Enum):
    """Enum for state of command chain\n
    Idle: Chain hasnt been started yet\n
    Running: Chain is currently executing blocks\n
    Paused: Chain has started but has been interrupted\n
    Finished: Chain has completed all blocks and is not looping
    """
    IDLE = auto()
    RUNNING = auto()
    PAUSED = auto()
    FINISHED = auto()

class CommandBlockResult(Enum):
    """Codes returned from executing command block callback\n
    (-1) DUMMY: Block called the dummy() function which has no implementation
    (0) SUCCESS: Only success flag, returned when command block successfully executes callback\n
    (1) GENERIC_FAIL: Standard fail when something went wrong\n
    (2) INVALID_OPERATION: Attempted to perform an operation that doesnt exist\n
    (3) PERMISSION_DENIED: Current user does not have permission to perform this operation\n 
    (4) SEND_FAIL: Failed to send data to ev3 brick from socket\n
    """
    DUMMY = -1
    SUCCESS = 0
    GENERIC_FAIL = 1
    INVALID_OPERATION = 2
    PERMISSION_DENIED = 3
    SEND_FAIL = 4

async def dummy(**kwargs) -> tuple[CommandBlockResult, str]:
    return CommandBlockResult.DUMMY, "Dummy callback cannot be called"

class CommandBlockBase(ABC):
    """
    Abstract command block class to be overridden
    """
    def __init__(self, **kwargs):
        self._name = kwargs.get("name", "command") #name of the command
        self._action = kwargs.get("action", "") #action to take
        self._subject = kwargs.get("subject", "") #subject to take action on if action uses subject
        self._conditions = kwargs.get("conditions", []) #conditions to take action
        self._parameters = kwargs.get("parameters", {}) #arguments given to block to work with
        self._delay = kwargs.get("delay", 0) #pause delay before executing block
        self._stoppable = kwargs.get("stoppable", False) #whether current block can be interrupt in between executing
        self._next = None #jump to index of next block after current block done executing
        self._exhaust = -1 #number of times this block can jump, cannot jump if -1 regardless of self._next, decrements after each jump, never decrements if None
        self._running = False #whether the block is running or stopped
        self._callback: Callable[..., Awaitable[tuple[CommandBlockResult, str]]] = dummy

        self._resources = {} #allocated resources that may need to be closed

    def get_next(self) -> int | None:
        """Returns index of next command block, or None if current command block is last in chain"""
        return self._next
    
    def can_jump(self) -> bool:
        """Whether the current block can jump"""
        if self._exhaust is None:
            return True
        if self._exhaust <= 0:
            return False
        self._exhaust -= 1
        return True

    @abstractmethod
    async def execute(self) -> tuple[CommandBlockResult, str]:
        """Cannot call execute() on CommandBlockBase class"""
        pass

    @abstractmethod
    def end(self) -> None:
        """Cannot call end() on CommandBlockBase class"""
        pass
    
class CommandBlock(CommandBlockBase):
    """
    Command block class that executes a command in a chain
    """
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    async def execute(self) -> tuple[CommandBlockResult, str]:
        """Runs the current block. callback() returns tuple of code and reason.\n 
        For successful execution, result, reason = tuple[CommandBlockResult.SUCCESS, \"\"]"""
        result, reason = await self._callback(**self._parameters)
        self.end()
        return result, reason

    def end(self) -> None:
        """Cleans up current block's resources if any"""
        pass #TODO: Implement end()

class ParallelBlock(CommandBlockBase):
    """
    Command block class that executes commands in parallel
    """
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

    async def execute(self) -> tuple[CommandBlockResult, str]:
        """Gathers async tasks and runs them in parallel"""
        return CommandBlockResult.DUMMY, "" #TODO: Implement end()

    def end(self) -> None:
        """Cleans up current block's resources if any"""
        pass #TODO: Implement end()

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
        self._current = 0

        self._pause_event = asyncio.Event() #cleared when paused, set when running
        self._pause_event.set()

    def append(self, block: CommandBlockBase):
        """Adds a block to the chain"""
        self._chain.append(block)
        self._nodes += 1

    async def run(self) -> None:
        """Runs the current chain"""
        if self._nodes <= 0: return
        
        if self._state != CommandChainState.PAUSED:
            self._current = 0
        self._state = CommandChainState.RUNNING

        try:
            while True:
                await self._pause_event.wait()

                if self._current < self._nodes:
                    await self._chain[self._current].execute()

                    self._current = self.next()
                else:
                    if self._loop:
                        self._current = 0
                        continue
                    else:
                        break
                
                await asyncio.sleep(0)
        finally:
            if self._current >= self._nodes and not self._loop:
                self._state = CommandChainState.FINISHED

    def next(self) -> int:
        """Moves to the target index if possible, otherwise proceeds to next block"""
        current_block = self._chain[self._current]
        next_index = current_block.get_next()

        if next_index is None or next_index == self._current + 1:
            return self._current + 1
        
        if current_block.can_jump():
            return next_index

        return self._current + 1

    def pause(self):
        """Pauses current chain execution"""
        self._state = CommandChainState.PAUSED
        self._pause_event.clear()

    def resume(self):
        """Resumes current chain execution"""
        self._state = CommandChainState.RUNNING
        self._pause_event.set()

    def read(self) -> list[str]:
        """Return the name of all the callback functions in order in the chain"""
        names = []
        for command in self._chain:
            names.append(command._callback.__name__)
        return names

class CommandQueue(janus.Queue):
    """
    Structure that holds multiple chains in a queue in order of creation
    """
    def __init__(self):
        super().__init__(maxsize=8)
        self._running_chain: CommandChain | None = None

    def clear_all(self) -> None:
        """Empties the queue and clears all chains"""
        while not self.sync_q.empty():
            self.sync_q.get_nowait()

    def clear_current(self) -> bool:
        """Resets currenty loaded chain and returns whether it was cleared successfully"""
        if self._running_chain:
            self._running_chain = None
            return True
        return False

    async def run_next(self) -> None:
        """Runs the next chain in the queue"""
        self._running_chain = await self.async_q.get() if not self._running_chain else self._running_chain

        if self._running_chain is None: return

        await self._running_chain.run()

        if self._running_chain._state == CommandChainState.FINISHED and self._running_chain._clear:
            self._running_chain = None

    async def interrupt(self) -> int | None:
        """Pauses current chain after currently executing block is done and return block number that just executed"""
        if self._running_chain is not None:
            self._running_chain.pause()
            return self._running_chain._current
        return None

    async def resume(self) -> None:
        """Continues off from where current chain paused"""
        if self._running_chain is not None:
            self._running_chain.resume()


