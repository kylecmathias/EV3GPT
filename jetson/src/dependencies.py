import sys
from pathlib import Path

DEPENDENCIES_PATH = Path(__file__).resolve().parent.parent / "dependencies"
DAV2_PATH = DEPENDENCIES_PATH / "Depth-Anything-V2"

def add_import(path: str | Path):
    if isinstance(path, Path):
        path = str(path)
    sys.path.append(path)