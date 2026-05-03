import os
import asyncio
import torch
from pathlib import Path
from speechbrain.inference.speaker import SpeakerRecognition
from insightface.app import FaceAnalysis
from safetensors.torch import save_file, load_file

from type_aliases import OptionalTensor

device = "cuda" if torch.cuda.is_available() else "cpu"
face_app = FaceAnalysis(name='tiny_face', providers=[f"{'CUDAExecutionProvider' if device == 'cuda' else 'CPUExecutionProvider'}"])
face_app.prepare(ctx_id=(0 if device == "cuda" else -1), det_size=(640, 640))
STORAGE_PATH = Path(__file__).parent.parent / "storage"

verification = SpeakerRecognition.from_hparams(source="speechbrain/spkrec-ecapa-voxceleb", run_opts={"device": device})
if not verification:
    raise ImportError("Failed to load speaker verification model")

class Audio:
    """Audio class for handling audio data and storing audio embeddings."""
    def __init__(self, embed: torch.Tensor):
        self._embedding = embed

    @classmethod
    async def create(cls, audio_tensor: torch.Tensor, embed: OptionalTensor = None) -> "Audio":
        if embed is not None:
            return cls(embed)
        
        encoded_embed = await asyncio.to_thread(cls._run_encoding, audio_tensor)
        return cls(encoded_embed) #type: ignore

    @staticmethod
    def _run_encoding(audio: torch.Tensor) -> OptionalTensor:
        def prepare(a):
            if not isinstance(a, torch.Tensor): 
                a = torch.tensor(a)
            if a.ndim == 1: 
                a = a.unsqueeze(0)
            return a.to(device).float()
        
        audio = prepare(audio)

        with torch.no_grad():
            return verification.encode_batch(audio) if verification else None

    def embedding(self) -> OptionalTensor:
        return self._embedding  
    
class Face:
    """Face class for storing face data and storing face embeddings."""
    def __init__(self, embed: torch.Tensor):
        self._embedding = embed

    @classmethod
    async def create(cls, face_tensor: torch.Tensor, embed: OptionalTensor = None) -> "Face":
        if embed is not None:
            return cls(embed)
        
        encoded_embed = await asyncio.to_thread(cls._run_encoding, face_tensor)
        return cls(encoded_embed) #type: ignore

    @staticmethod
    def _run_encoding(face: torch.Tensor) -> OptionalTensor:
        def prepare(f):
            if not isinstance(f, torch.Tensor): 
                f = torch.tensor(f)
            if f.ndim == 1: 
                f = f.unsqueeze(0)
            return f.to(device).float()
        
        face = prepare(face)

        with torch.no_grad():
            face_np = face.cpu().numpy().transpose(1, 2, 0)
            face_np = face_np[:, :, ::-1] 
            faces = face_app.get(face_np)

            return torch.tensor(faces[0].embedding).to(device).unsqueeze(0) if faces else None

    def embedding(self) -> OptionalTensor:
        return self._embedding

class Profile:
    """Overall profile for matching biometric attributes to person"""
    def __init__(self, name: str, face_embed: OptionalTensor = None, voice_embed: OptionalTensor = None):
        self._name = name
        self._face_embed = face_embed
        self._voice_embed = voice_embed
        self._threshold = 0.7

    def add_face(self, face: Face | torch.Tensor, replace: bool) -> None:
        if self._face_embed and not replace: return

        self._face_embed = face.embedding() if isinstance(face, Face) else face

    def add_voice(self, voice: Audio | torch.Tensor, replace: bool) -> None:
        if self._voice_embed and not replace: return

        self._voice_embed = voice.embedding() if isinstance(voice, Audio) else voice

    async def verify_face(self, face: Face | torch.Tensor) -> tuple[bool, float]:
        if self._face_embed is None: return False, 0.0
        
        feat = face.embedding() if isinstance(face, Face) else face
        if feat is None: return False, 0.0
        
        sim = torch.nn.functional.cosine_similarity(self._face_embed, feat).item()
        return sim > self._threshold, sim

    async def verify_voice(self, voice: Audio | torch.Tensor) -> tuple[bool, float]:
        if self._voice_embed is None: return False, 0.0

        feat = voice.embedding() if isinstance(voice, Audio) else voice
        if feat is None: return False, 0.0

        sim = torch.nn.functional.cosine_similarity(self._voice_embed, feat).item()
        return sim > self._threshold, sim
    
class ProfileManager:
    """Singleton class for storing and operating on profiles"""
    _instance = None
    _initialized = False

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(ProfileManager, cls).__new__(cls)
        return cls._instance

    def __init__(self):
        if self._initialized: return

        self._profile_path = STORAGE_PATH / "profiles.safetensors"
        self._profiles: dict[str, Profile] = {}
        
        self._initialized = True

    def save_profiles(self):
        tensors = {}

        for name, profile in self._profiles.items():
            if profile._face_embed is not None:
                tensors[f"{name}_face"] = profile._face_embed
            if profile._voice_embed is not None:
                tensors[f"{name}_voice"] = profile._voice_embed

        STORAGE_PATH.mkdir(parents=True, exist_ok=True)
        save_file(tensors, self._profile_path)

    async def load_profiles(self):
        if not self._profile_path.exists(): return

        raw_data = load_file(self._profile_path)
        names = {key.rsplit('_', 1)[0] for key in raw_data.keys()}

        for name in names:
            f_em = raw_data.get(f"{name}_face").to(device) if f"{name}_face" in raw_data else None #type: ignore
            v_em = raw_data.get(f"{name}_voice").to(device) if f"{name}_voice" in raw_data else None #type: ignore
            self._profiles[name] = Profile(name, face_embed=f_em, voice_embed=v_em)

    async def match_biometrics(self, face: Face | OptionalTensor = None, voice: Audio | OptionalTensor = None) -> tuple[str, float]:
        best_name, best_score = "", 0.0

        with torch.no_grad():
            for name, profile in self._profiles.items():
                f_sim, v_sim = 0.0, 0.0
                f_match, v_match = True, True
                
                if face is not None:
                    f_match, f_sim = await profile.verify_face(face)
                if voice is not None:
                    v_match, v_sim = await profile.verify_voice(voice)
                
                if f_match and v_match:
                    score = f_sim if voice is None else (v_sim if face is None else (f_sim + v_sim) / 2)
                    if score > best_score:
                        best_name, best_score = name, score

        return best_name, best_score

    def get_profile(self, name: str) -> Profile:
        if name not in self._profiles:
            raise KeyError(f"User {name} not registered.")
        return self._profiles[name]