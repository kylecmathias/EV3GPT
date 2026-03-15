from faster_whisper import WhisperModel
from speechbrain.inference.speaker import SpeakerRecognition
import torch

device = "cuda" if torch.cuda.is_available() else "cpu"
compute_type = "int8_float16" if device == "cuda" else "int8"
verification = SpeakerRecognition.from_hparams(source="speechbrain/spkrec-ecapa-voxceleb", run_opts={"device": device})

if not verification:
    raise ImportError("Failed to load speaker verification model")

model = WhisperModel("tiny.en", device=device, compute_type=compute_type)

class VoiceProfile:
    """
    Voice profiles for matching audio to people
    """
    def __init__(self, name: str, audio, threshold: float = 0.7):
        self._name = name
        self._threshold = threshold
        audio = self._prepare_audio(audio)
        with torch.no_grad():
            self._embedding = verification.encode_batch(audio) if verification else None

    def _prepare_audio(self, audio) -> torch.Tensor:
        """
        Helper to prepare audio for format that speechbrain wont crash out at
        """
        if not isinstance(audio, torch.Tensor):
            audio = torch.tensor(audio)
        if audio.ndim == 1:
            audio = audio.unsqueeze(0)
        return audio.to(device).float()

    def verify(self, audio) -> tuple[bool, float]:
        """
        Verify that the audio matches the profile to check which profile it is
        """
        audio = self._prepare_audio(audio)
        with torch.no_grad():
            embedding = verification.encode_batch(audio) if verification else None
        if embedding is None or self._embedding is None:
            print(f"WARNING: One of embeddings is None, skipping verification. Profile embed: {self._embedding}, input embed: {embedding}")
            return False, 0.0
        
        similarity = torch.nn.functional.cosine_similarity(self._embedding, embedding).item()
        return similarity > self._threshold, similarity
    
class FaceProfile:
    """
    Face profile class for matching face to voice
    """            
    def __init__(self, face: torch.Tensor, voice: VoiceProfile, threshold: float = 0.7):
        self._face = face
        self._voice = voice
        self._threshold = threshold