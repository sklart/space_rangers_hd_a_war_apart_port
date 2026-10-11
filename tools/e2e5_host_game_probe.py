"""Run an already built E2E-5 host ELF against installed assets in isolation.

This probe reports only observed startup and software-frame checkpoints. It
does not claim to exercise New Game or the complete effective Switch build.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / "port/switch/build/e2e5-host-game/runs"
FRAME = re.compile(
    r"HOST first software frame width=(\d+) height=(\d+) rgb565_hash=([0-9a-f]{16})"
)


def tail(path: Path, limit: int = 5000) -> str:
    return path.read_text(encoding="utf-8", errors="replace")[-limit:] if path.is_file() else ""


def run(binary: Path, game_root: Path, library_dir: Path, timeout: int,
        crash_trace: Path | None, continue_after_frame: bool) -> dict:
    binary = binary.resolve(strict=True)
    game_root = game_root.resolve(strict=True)
    library_dir = library_dir.resolve(strict=True)
    if not binary.is_file() or not game_root.is_dir() or not library_dir.is_dir():
        raise ValueError("binary, game root, and library directory must exist")
    RUNS.mkdir(parents=True, exist_ok=True)
    stage = RUNS / uuid4().hex
    stage.mkdir()
    game_log = stage / "user/config/########.log"
    e2e_log = stage / "sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log"
    stderr_path = stage / "stderr.txt"
    env = os.environ.copy()
    env.update(E2E5_GAME_ROOT=str(game_root), SDL_VIDEODRIVER="dummy",
               SDL_AUDIODRIVER="dummy", LD_LIBRARY_PATH=str(library_dir))
    if crash_trace:
        env["LD_PRELOAD"] = str(crash_trace.resolve(strict=True))
    began = time.monotonic()
    seen_frame: re.Match[str] | None = None
    stopped_by_probe = False
    with stderr_path.open("w", encoding="utf-8") as stderr:
        process = subprocess.Popen([str(binary)], cwd=stage, env=env,
                                   stdout=subprocess.DEVNULL, stderr=stderr)
        while process.poll() is None and time.monotonic() - began < timeout:
            seen_frame = FRAME.search(tail(e2e_log))
            if seen_frame and not continue_after_frame:
                stopped_by_probe = True
                process.terminate()
                break
            time.sleep(0.5)
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    if not seen_frame:
        seen_frame = FRAME.search(tail(e2e_log))
    elapsed = round(time.monotonic() - began, 2)
    if seen_frame:
        state = "FIRST_SOFTWARE_FRAME_OBSERVED"
    elif process.returncode is not None and process.returncode < 0:
        state = "SIGNAL_BEFORE_FIRST_FRAME"
    elif elapsed >= timeout:
        state = "TIMEOUT_BEFORE_FIRST_FRAME"
    else:
        state = "EXIT_BEFORE_FIRST_FRAME"
    result = {
        "schema": 1,
        "level": "ORIGINAL GAME HOST PROCESS; PARTIAL EFFECTIVE SOURCE",
        "status": state,
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "elapsed_seconds": elapsed,
        "returncode": process.returncode,
        "stopped_by_probe": stopped_by_probe,
        "first_software_frame": (
            {"width": int(seen_frame[1]), "height": int(seen_frame[2]),
             "rgb565_hash": seen_frame[3]} if seen_frame else None
        ),
        "game_settings_first_frame": "NOT RUN",
        "new_game": "NOT RUN",
        "audio": "NOT RUN",
        "stage": str(stage),
        "game_log_tail": tail(game_log),
        "e2e_log_tail": tail(e2e_log),
        "stderr_tail": tail(stderr_path),
    }
    (stage / "report.json").write_text(json.dumps(result, indent=2) + "\n",
                                       encoding="utf-8")
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--game-root", type=Path,
                        default=Path(os.environ["GAME_ROOT"]) if "GAME_ROOT" in os.environ else None)
    parser.add_argument("--library-dir", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=100)
    parser.add_argument("--crash-trace", type=Path)
    parser.add_argument("--continue-after-frame", action="store_true")
    args = parser.parse_args()
    if args.game_root is None or args.timeout <= 0:
        parser.error("--game-root or GAME_ROOT and a positive --timeout are required")
    print(json.dumps(run(args.binary, args.game_root, args.library_dir,
                         args.timeout, args.crash_trace,
                         args.continue_after_frame), indent=2))


if __name__ == "__main__":
    main()
