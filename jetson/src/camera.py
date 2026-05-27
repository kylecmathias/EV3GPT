import cv2
import subprocess
import numpy as np
import threading
import pyaudio
import time
import janus 
import asyncio

import config
from dependencies import secrets

RTSP_URL = f"rtsp://{secrets("username", "camera")}:{secrets("password", "camera")}@{config.RTSP_ADDR}:{config.RTSP_PORT}/{config.RTSP_STREAM}"
WIDTH, HEIGHT = 640, 480
CHANNELS = 1
RATE = 16000
CHUNK = 512
MAX_DIV = 2
RECONNECT_DELAY = 2

class CameraStream:
    """
    Class for camera and audio streams. 
    Call start() to begin, stop() to end.
    Access latest frame via get_frame(), latest audio chunk via get_audio().
    Automatically reconnects if the stream goes down while running.
    """
    _instance = None
    _initialized = False

    def __new__(cls, *args, **kwargs):
        if cls._instance is None:
            cls._instance = super(CameraStream, cls).__new__(cls)
        return cls._instance

    def __init__(self, url: str = RTSP_URL):
        if self._initialized: return

        self._url = url
        self._running = False

        self._frame_queue: janus.Queue[np.ndarray] = janus.Queue(maxsize=1)
        self._audio_queue: janus.Queue[bytes] = janus.Queue(maxsize=(RATE // CHUNK) // MAX_DIV)

        self._video_thread: threading.Thread | None = None
        self._audio_thread: threading.Thread | None = None

        self._initialized = True

    def start(self) -> None:
        """Start video and audio streams."""
        if self._running:
            return
        self._running = True
        self._video_thread = threading.Thread(target=self._video_pipeline, daemon=True)
        self._audio_thread = threading.Thread(target=self._audio_pipeline, daemon=True)
        self._video_thread.start()
        self._audio_thread.start()

    def stop(self) -> None:
        """Stop video and audio streams."""
        self._running = False

    def sync_get_frame(self) -> np.ndarray | None:
        """Get the latest video frame, or None if unavailable."""
        try:
            return self._frame_queue.sync_q.get_nowait()
        except janus.SyncQueueEmpty:
            return None

    def sync_get_audio(self) -> bytes | None:
        """Get the latest audio chunk, or None if unavailable."""
        try:
            return self._audio_queue.sync_q.get_nowait()
        except janus.SyncQueueEmpty:
            return None
        
    async def async_get_frame(self) -> np.ndarray | None:
        """Get the latest video frame, or None if unavailable."""
        try:
            return self._frame_queue.async_q.get_nowait()
        except janus.AsyncQueueEmpty:
            return None

    async def async_get_audio(self) -> bytes | None:
        """Get the latest audio chunk, or None if unavailable."""
        try:
            return self._audio_queue.async_q.get_nowait()
        except janus.AsyncQueueEmpty:
            return None
        
    def _put_frame(self, frame: np.ndarray) -> None:
        """Drop stale frame and put new one."""
        try:
            self._frame_queue.sync_q.get_nowait()
        except janus.SyncQueueEmpty:
            pass
        self._frame_queue.sync_q.put_nowait(frame)

    def _put_audio(self, data: bytes) -> None:
        """Drop stale chunk and put new one."""
        try:
            self._audio_queue.sync_q.get_nowait()
        except janus.SyncQueueEmpty:
            pass
        self._audio_queue.sync_q.put_nowait(data)

    def _video_pipeline(self) -> None:
        cmd = [
            'ffmpeg', '-hide_banner', '-loglevel', 'error',
            '-fflags', 'nobuffer',
            '-flags', 'low_delay',
            '-rtsp_transport', 'tcp',
            '-probesize', '32',
            '-analyzeduration', '0',
            '-hwaccel', 'cuda',
            '-i', self._url,
            '-an', '-sn',
            '-f', 'rawvideo', '-pix_fmt', 'bgr24',
            '-vcodec', 'rawvideo',
            '-vsync', '0',
            'pipe:1'
        ]
        frame_size = WIDTH * HEIGHT * 3

        while self._running:
            print("[video] Connecting...")
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, bufsize=0)

            def reader(proc: subprocess.Popen):
                while self._running:
                    raw = b''
                    while len(raw) < frame_size:
                        chunk = proc.stdout.read(frame_size - len(raw)) if proc.stdout else b''
                        if not chunk:
                            return
                        raw += chunk
                    frame = np.frombuffer(raw, dtype=np.uint8).reshape((HEIGHT, WIDTH, 3))
                    self._put_frame(frame)

            reader_thread = threading.Thread(target=reader, args=(process,), daemon=True)
            reader_thread.start()

            try:
                reader_thread.join()
                if self._running:
                    raise ConnectionError("Stream ended")
            except ConnectionError:
                print(f"[video] Stream lost, reconnecting in {RECONNECT_DELAY}s...")
                time.sleep(RECONNECT_DELAY)
            finally:
                process.terminate()
                process.wait()

    def _audio_pipeline(self) -> None:
        cmd = [
            'ffmpeg', '-hide_banner', '-loglevel', 'error',
            '-fflags', 'nobuffer+discardcorrupt',
            '-rtsp_transport', 'tcp',
            '-probesize', '32',
            '-analyzeduration', '0',
            '-i', self._url,
            '-map', '0:a:0',
            '-acodec', 'pcm_s16le',
            '-ar', str(RATE),
            '-ac', str(CHANNELS),
            '-f', 's16le',
            'pipe:1'
        ]

        p = pyaudio.PyAudio()
        stream = p.open(format=pyaudio.paInt16, channels=CHANNELS, rate=RATE, output=True, frames_per_buffer=CHUNK)

        while self._running:
            print("[audio] Connecting...")
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, bufsize=0)

            def reader(proc: subprocess.Popen):
                while self._running:
                    data = proc.stdout.read(CHUNK * 2) if proc.stdout else b''
                    if not data:
                        break
                    self._put_audio(data)

            reader_thread = threading.Thread(target=reader, args=(process,), daemon=True)
            reader_thread.start()

            try:
                reader_thread.join()
                if self._running:
                    raise ConnectionError("Stream ended")
            except ConnectionError:
                print(f"[audio] Stream lost, reconnecting in {RECONNECT_DELAY}s...")
                time.sleep(RECONNECT_DELAY)
            finally:
                process.terminate()
                process.wait()

        stream.stop_stream()
        stream.close()
        p.terminate()
