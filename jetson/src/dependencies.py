import sys
from pathlib import Path
import tomllib

ROOT = Path(__file__).resolve().parent.parent
DEPENDENCIES_PATH = ROOT / "dependencies"
DAV2_PATH = DEPENDENCIES_PATH / "Depth-Anything-V2"
SECRETS_PATH = ROOT / "secrets.toml"

def add_import(path: str | Path):
    if isinstance(path, Path):
        path = str(path)
    sys.path.append(path)

def secrets(name: str, table: str = "", guarantee: bool = True):
    with open(SECRETS_PATH, "rb") as f:
        data = tomllib.load(f)

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