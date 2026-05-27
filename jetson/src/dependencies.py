import sys
import tomllib

from pathlib import Path
from type_aliases import Secret

_SECRETS_CACHE = "secrets_dict"

ROOT = Path(__file__).resolve().parent.parent #jetson/
DEPENDENCIES_PATH = ROOT / "dependencies" #jetson/dependencies/
SECRETS_PATH = ROOT / "secrets.toml" #jetson/secrets.toml
DAV2_PATH = DEPENDENCIES_PATH / "Depth-Anything-V2" #jetson/dependencies/Depth-Anything-V2/
AI_FOLDER = ROOT / "src" / "AI"

PIPER_TTS_WEIGHTS = AI_FOLDER / "piper-tts.onnx" #jetson/src/AI/Piper-TTS.onnx
DAV2_WEIGHTS = AI_FOLDER / "dav2.pth" #jetson/src/AI/dav2.pth

def add_import(path: str | Path):
    if isinstance(path, Path):
        path = str(path)
    sys.path.append(path)

def secrets(name: str, table: str = "", guarantee: bool = True, reread: bool = False) -> Secret:
    if not hasattr(secrets, _SECRETS_CACHE) or reread:
        with open(SECRETS_PATH, "rb") as f:
            secrets.__dict__[_SECRETS_CACHE] = tomllib.load(f)

    data = secrets.__dict__[_SECRETS_CACHE]

    if table == "":
        secret = data.get(name)
    else:
        if table in data and isinstance(data[table], dict):
            secret = data[table].get(name)
        else:
            raise RuntimeError(f"Table: [{table}] not in secrets.toml")

    if guarantee and not secret:
        raise RuntimeError(f"Secret: [{name}] not in secrets.toml")
    
    return secret

