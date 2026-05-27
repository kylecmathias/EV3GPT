import asyncio
import numpy as np
import torch
import json
from faster_whisper import WhisperModel
from speechbrain.inference.speaker import SpeakerRecognition

from biometrics import Audio, Face, Profile, ProfileManager
from builder import chain_builder
from camera import CameraStream
from commands import CommandQueue
from llm import LLMAgent
from audio import AudioStream, audio_priority

device = "cuda" if torch.cuda.is_available() else "cpu"
compute_type = "int8_float16" if device == "cuda" else "int8"

model, utils = torch.hub.load(repo_or_dir='snakers4/silero-vad', model='silero_vad', force_reload=False, onnx=True) # type: ignore
(get_speech_timestamps, save_audio, read_audio, VADIterator, collect_chunks) = utils

vad_iterator = VADIterator(model, min_silence_duration_ms=1000, threshold=0.5)

whisper_model = WhisperModel("tiny.en", device=device, compute_type=compute_type)

class Interaction:
    """Class for interacting with the robot and llm using audio and video"""
    _instance = None
    _initialized = False

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(Interaction, cls).__new__(cls)
        return cls._instance
    
    def __init__(self):
        if self._initialized: return

        self._model = WhisperModel("tiny.en", device=device, compute_type=compute_type)
        self._audio_buffer: list[np.ndarray] = []
        self._profile_manager = ProfileManager()
        self._cmd_queue = CommandQueue()

        self._initialized = True

    async def capture_audio(self):
        """
        Continuously captures audio and puts it in the queue when speech ends.
        In a real robot, replace the dummy 'get_audio' with a stream from PyAudio or ALSA.
        """

        cam = CameraStream()

        while True:
            audio_bytes = await cam.async_get_audio()

            if audio_bytes is None:
                await asyncio.sleep(0.01)
                continue

            audio_chunk = np.frombuffer(audio_bytes, dtype=np.int16).astype(np.float32) / 32768.0

            self._audio_buffer.append(audio_chunk)

            speech_dict = vad_iterator(torch.from_numpy(audio_chunk), return_seconds=True)

            if speech_dict:
                if "start" in speech_dict:
                    self._audio_buffer = [audio_chunk]
                if "end" in speech_dict:
                    full_audio = np.concatenate(self._audio_buffer)
                    self._audio_buffer = [] 
                    asyncio.create_task(self.process_audio(full_audio))

    async def process_audio(self, audio_data: np.ndarray):
        """Task for transcribing audio and assigning profile"""
        audio_tensor = torch.from_numpy(audio_data).float()

        speaker_task = asyncio.create_task(self._identify_speaker(audio_tensor))
        transcribe_task = asyncio.to_thread(self._model.transcribe, audio_data)

        speaker, (segments, _) = await asyncio.gather(speaker_task, transcribe_task)
        
        text = " ".join([s.text for s in segments]).strip()
        if not text: return
        
        llm = LLMAgent()
        response_text = await llm.process_text(text, speaker)
        if not response_text: return

        try:
            clean = response_text.strip().strip("```json").strip("```").strip()
            response = json.loads(clean)
        except json.JSONDecodeError:
            print(f"LLM response parse error: {response_text}")
            return
        
        speak_text = response.get("speak", "")
        has_command = response.get("command", False)

        if speak_text:
            asyncio.create_task(self._speak(speak_text))
        
        if has_command:
            asyncio.create_task(self._dispatch_command(text))
            
    async def _dispatch_command(self, text: str) -> None:
        """Normalize text via command parser and queue the resulting chain."""
        llm = LLMAgent()
        normalized = await llm.process_command(text)

        if not normalized:
            return

        chain = chain_builder(normalized)
        if chain is None:
            print(f"chain_builder returned None for: {normalized}")
            return

        await self._cmd_queue.async_q.put(chain)

    async def _identify_speaker(self, audio_tensor):
        """Uses audio to identify speaker, and face if face exists"""
        try:
            current_audio = await Audio.create(audio_tensor)
            name, score = await self._profile_manager.match_biometrics(voice=current_audio)
            if name == "" and score == 0.0:
                return "!Speaker not registered"
            else:
                return self._profile_manager.get_profile(name)
        except Exception as e:
            print(f"Speaker ID Error: {e}")
            return f"Error identifying speaker: {e}"
        
    async def _speak(self, text: str, priority: int = audio_priority(2), interrupt: bool = False) -> None:
        await AudioStream().speak(text, priority, interrupt)

    async def run_commands(self) -> None:
        """Consumer loop for the command queue — run as a concurrent task."""
        while True:
            await self._cmd_queue.run_next()
