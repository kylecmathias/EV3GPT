import socket
import struct
import asyncio
import numpy as np
import soundfile as sf
import soxr
from piper.voice import PiperVoice, AudioChunk

import config
from dependencies import PIPER_TTS_WEIGHTS

PAYLOAD_SIZE = 512
PACKET_SIZE = PAYLOAD_SIZE + 1
TARGET_RATE = 8000
BYTES_PER_SAMPLE = 2

PRIORITY_SHIFT = 5
FLAG_INTERRUPT = 0x10
FLAG_FLUSH = 0x08
FLAG_START = 0x04
FLAG_END = 0x02

def audio_priority(priority: int = 2):
    """Clamps audio priority to 3 bit range (0-7)"""
    return max(min(priority, 7), 0)

class AudioStream:
    """
    Class for TTS generation to stream to EV3 brick. 
    Uses Piper for TTS, resamples to 8kHz, streams over UDP to port 8889.
    """
    _instance = None
    _initialized = False

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(AudioStream, cls).__new__(cls)
        return cls._instance
    
    def __init__(self):
        if self._initialized: return

        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._addr = None
        self._loop = asyncio.get_event_loop()
        self._voice = PiperVoice.load(PIPER_TTS_WEIGHTS)
        self._lock = asyncio.Lock()

        self._initialized = True
    
    def set_addr(self, addr: tuple[str, int]) -> None:
        """Set EV3 address"""
        self._addr = (addr[0], config.AUDIO_PORT)

    async def speak(self, text: str, priority: int = audio_priority(2), interrupt: bool = False) -> None:
        """Generate TTS and stream to EV3"""
        if not self._addr:
            print("No EV3 address set for audio")
            return
        
        async with self._lock:
            pcm = await asyncio.to_thread(self._synthesize, text)
            if pcm is None:
                return
            await self._stream(pcm, priority, interrupt)

    def _synthesize(self, text: str) -> np.ndarray | None:
        """Run Piper TTS and resample to 8kHz int16 mono"""
        try:
            audio_bytes = b"".join(bytes(audio_chunk.audio_int16_bytes) for audio_chunk in self._voice.synthesize(text))
            pcm = np.frombuffer(audio_bytes, dtype=np.int16).astype(np.float32) / 32768.0
            
            piper_rate = self._voice.config.sample_rate
            if piper_rate != TARGET_RATE:
                pcm = soxr.resample(pcm, piper_rate, TARGET_RATE)

            return (pcm * 32767).astype(np.int16)
        except Exception as e:
            print(f"TTS error: {e}")
            return
        
    async def _stream(self, pcm: np.ndarray, priority: int, interrupt: bool) -> None:
        """Packetize and send PCM to EV3"""
        raw = pcm.tobytes()
        total = len(raw)
        offset = 0
        is_first = True

        while offset < total:
            chunk = raw[offset:offset + PAYLOAD_SIZE]

            if len(chunk) < PAYLOAD_SIZE:
                chunk = chunk + b"\x00" * (PAYLOAD_SIZE - len(chunk))

            is_last = (offset + PAYLOAD_SIZE) >= total

            control = (priority << PRIORITY_SHIFT) & 0xE0
            if is_first and interrupt:
                control |= FLAG_INTERRUPT
            if is_first:
                control |= FLAG_START
            if is_last:
                control |= FLAG_END

            packet = struct.pack("B", control) + chunk

            try:
                if not self._addr:
                    return
                await self._loop.sock_sendto(self._sock, packet, self._addr) 
            except OSError as e:
                print(f"Audio send error: {e}")
                return
            
            offset += PAYLOAD_SIZE
            is_first = False
            await asyncio.sleep(0)

    async def flush(self) -> None:
        """Tell EV3 to clear audio queue immediately"""
        if not self._addr:
            return
        control = FLAG_FLUSH
        packet = struct.pack("B", control) + b"\x00" * PAYLOAD_SIZE
        try:
            await self._loop.sock_sendto(self._sock, packet, self._addr)
        except OSError as e:
            print(f"Audio flush error: {e}")

            
            