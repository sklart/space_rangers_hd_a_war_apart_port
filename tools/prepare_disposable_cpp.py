"""Refresh a build-local C++ source copy without touching the pinned submodule."""

from pathlib import Path
import shutil


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "upstream/cpp"
DESTINATION = ROOT / "port/switch/build/disposable-cpp"

for tree in ("src", "runtime"):
    for source in (SOURCE / tree).rglob("*"):
        if not source.is_file():
            continue
        target = DESTINATION / source.relative_to(SOURCE)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
