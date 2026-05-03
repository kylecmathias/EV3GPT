import asyncio
import numpy as np
import torch
from faster_whisper import WhisperModel
from speechbrain.inference.speaker import SpeakerRecognition

from biometrics import Audio, Face, Profile, ProfileManager

device = "cuda" if torch.cuda.is_available() else "cpu"
compute_type = "int8_float16" if device == "cuda" else "int8"

verification = SpeakerRecognition.from_hparams(source="speechbrain/spkrec-ecapa-voxceleb", run_opts={"device": device})
if not verification:
    raise ImportError("Failed to load speaker verification model")

model, utils = torch.hub.load(repo_or_dir='snakers4/silero-vad', model='silero_vad', force_reload=False, onnx=True) # type: ignore
(get_speech_timestamps, save_audio, read_audio, VADIterator, collect_chunks) = utils

vad_iterator = VADIterator(model, min_silence_duration_ms=1000, threshold=0.5)

whisper_model = WhisperModel("tiny.en", device=device, compute_type=compute_type)

class InteractionProcessor:
    """Class for interacting with the robot and llm using audio and video"""
    _instance = None
    _initialized = False

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(InteractionProcessor, cls).__new__(cls)
        return cls._instance
    
    def __init__(self):
        if self._initialized: return

        self._model = WhisperModel("tiny.en", device=device, compute_type=compute_type)
        self._audio_buffer = []
        self._profile_manager = ProfileManager()

        self._initialized = True

    async def capture_audio(self):
        """
        Continuously captures audio and puts it in the queue when speech ends.
        In a real robot, replace the dummy 'get_audio' with a stream from PyAudio or ALSA.
        """

        sampling_rate = 16000
        sample_size = 512

        while True:
            audio_chunk = np.array([]) #TODO: Await get audio chunk

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
        if not text:
            return

        llm_task = self.dummy_llm_task(text, speaker)      
        cmd_task = self.dummy_cmd_task(text) 

        llm_response, cmd_result = await asyncio.gather(llm_task, cmd_task)

        await self.dummy_speak(llm_response)

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

    async def dummy_llm_task(self, text, biometrics):
        raise NotImplementedError()
    
    async def dummy_cmd_task(self, text):
        raise NotImplementedError()

    async def dummy_speak(self, response):
        raise NotImplementedError()
    
    async def dummy_id_speaker(self, audio_tensor):
        raise NotImplementedError()
