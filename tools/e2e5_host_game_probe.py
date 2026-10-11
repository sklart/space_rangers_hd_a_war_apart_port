"""Run an already built E2E-5 host ELF against installed assets in isolation.

This probe reports observed startup, screen, and software-frame checkpoints.
Its optional New Game input uses the original menu's Return-key handler.
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
        crash_trace: Path | None, continue_after_frame: bool,
        auto_new_game: bool = False, auto_start_game: bool = False) -> dict:
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
    if auto_new_game or auto_start_game:
        env["E2E5_HOST_AUTO_NEW_GAME"] = "1"
    if auto_start_game:
        env["E2E5_HOST_AUTO_START_GAME"] = "1"
    if crash_trace:
        env["LD_PRELOAD"] = str(crash_trace.resolve(strict=True))
    began = time.monotonic()
    seen_frame: re.Match[str] | None = None
    seen_new_game_frame = False
    seen_introduction_frame = False
    seen_generation_complete = False
    stopped_by_probe = False
    timed_out = False
    with stderr_path.open("w", encoding="utf-8") as stderr:
        process = subprocess.Popen([str(binary)], cwd=stage, env=env,
                                   stdout=subprocess.DEVNULL, stderr=stderr)
        while process.poll() is None and time.monotonic() - began < timeout:
            if seen_frame is None and e2e_log.is_file():
                seen_frame = FRAME.search(e2e_log.read_text(encoding="utf-8", errors="replace"))
            if (auto_new_game or auto_start_game) and e2e_log.is_file():
                current = e2e_log.read_text(encoding="utf-8", errors="replace")
                new_game = current.find("screen dispatch id=33 postload=")
                seen_new_game_frame = (new_game >= 0 and
                    "screen Run first frame attempted" in current[new_game:])
                introduction = current.find("screen dispatch id=3 postload=", new_game)
                seen_introduction_frame = (introduction >= 0 and
                    "screen Run first frame attempted" in current[introduction:])
                seen_generation_complete = "generation stage=8 status=complete" in current
            if ((seen_frame and not continue_after_frame and
                 not auto_new_game and not auto_start_game) or
                    (auto_new_game and not auto_start_game and seen_new_game_frame) or
                    (auto_start_game and seen_generation_complete and
                     seen_introduction_frame)):
                stopped_by_probe = True
                process.terminate()
                break
            time.sleep(0.5)
        if process.poll() is None and not stopped_by_probe:
            timed_out = True
            stopped_by_probe = True
            process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    if not seen_frame and e2e_log.is_file():
        seen_frame = FRAME.search(e2e_log.read_text(encoding="utf-8", errors="replace"))
    elapsed = round(time.monotonic() - began, 2)
    stderr_tail = tail(stderr_path)
    full_stderr = stderr_path.read_text(encoding="utf-8", errors="replace") if stderr_path.is_file() else ""
    crashed = not stopped_by_probe and (
        (process.returncode is not None and process.returncode < 0) or
        "E2E5 HOST SIGNAL" in full_stderr)
    if crashed:
        state = "CRASH_AFTER_FIRST_FRAME" if seen_frame else "CRASH_BEFORE_FIRST_FRAME"
    elif not stopped_by_probe and process.returncode not in (None, 0):
        state = "PROCESS_ERROR_AFTER_FIRST_FRAME" if seen_frame else "PROCESS_ERROR_BEFORE_FIRST_FRAME"
    elif seen_frame:
        state = "FIRST_SOFTWARE_FRAME_OBSERVED"
    elif timed_out:
        state = "TIMEOUT_BEFORE_FIRST_FRAME"
    elif process.returncode is not None and process.returncode < 0:
        state = "SIGNAL_BEFORE_FIRST_FRAME"
    else:
        state = "EXIT_BEFORE_FIRST_FRAME"
    full_e2e_log = e2e_log.read_text(encoding="utf-8", errors="replace") if e2e_log.is_file() else ""
    full_game_log = game_log.read_text(encoding="utf-8", errors="replace") if game_log.is_file() else ""
    dispatches = [int(screen_id) for screen_id in
                  re.findall(r"screen dispatch id=(\d+) postload=\d+", full_e2e_log)]
    new_game = full_e2e_log.find("screen dispatch id=33 postload=")
    seen_new_game_frame = new_game >= 0 and (
        "screen Run first frame attempted" in full_e2e_log[new_game:])
    introduction = full_e2e_log.find("screen dispatch id=3 postload=", new_game)
    seen_introduction_frame = introduction >= 0 and (
        "screen Run first frame attempted" in full_e2e_log[introduction:])
    generation_stages = [int(stage) for stage in re.findall(
        r"generation stage=(\d+) status=(?:running|complete)", full_e2e_log)]
    generation_failure = re.search(r"generation stage=(\d+) status=failed", full_e2e_log)
    main_menu_ready = bool(re.search(
        r"screen dispatch id=1 postload=\d+\n\[E2E\] screen Run OnOpen begin\n"
        r"\[E2E\] screen Run OnOpen ready", full_e2e_log))
    ring_rows = [(int(played), int(underrun), int(rejected)) for played, underrun, rejected in
                 re.findall(r"\[MUSIC\] ring played_frames=(\d+) underrun_frames=(\d+) "
                            r"rejected_writes=(\d+)", full_game_log)]
    result = {
        "schema": 1,
        "level": "ORIGINAL GAME HOST PROCESS",
        "effective_source_scope": "UNVERIFIED BY THIS PROBE",
        "status": state,
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "elapsed_seconds": elapsed,
        "returncode": process.returncode,
        "stopped_by_probe": stopped_by_probe,
        "crashed": crashed,
        "timed_out": timed_out,
        "first_software_frame": (
            {"width": int(seen_frame[1]), "height": int(seen_frame[2]),
             "rgb565_hash": seen_frame[3]} if seen_frame else None
        ),
        "game_settings_first_frame": (
            "PRESENTATION ATTEMPT OBSERVED" if seen_new_game_frame else "NOT OBSERVED"),
        "new_game": ("SCREEN DISPATCH OBSERVED" if 33 in dispatches else
                     "INPUT REQUESTED; NO DISPATCH" if auto_new_game or auto_start_game
                     else "NOT RUN"),
        "auto_new_game_input": auto_new_game,
        "auto_start_game_input": auto_start_game,
        "generation_stages": generation_stages,
        "generation_failure_stage": (int(generation_failure[1]) if generation_failure else None),
        "settings_saved": "game_settings action=apply settings=saved" in full_e2e_log,
        "generation_worker_started": "generation worker=start-requested" in full_e2e_log,
        "generation_complete": "generation stage=8 status=complete" in full_e2e_log,
        "introduction_first_frame": (
            "PRESENTATION ATTEMPT OBSERVED" if seen_introduction_frame else "NOT OBSERVED"),
        "screen_dispatches": dispatches,
        "main_menu_on_open_ready": main_menu_ready,
        "audio": {
            "sdl_device_ready": "[AUDIO] SDL device ready" in full_game_log,
            "music_workers_ready": "[AUDIO] music workers ready" in full_game_log,
            "music_ring_max_played_frames": max((row[0] for row in ring_rows), default=0),
            "music_ring_max_underrun_frames": max((row[1] for row in ring_rows), default=0),
            "music_ring_max_rejected_writes": max((row[2] for row in ring_rows), default=0),
            "pcm_output_verified": False,
        },
        "stage": str(stage),
        "game_log_tail": tail(game_log),
        "e2e_log_tail": tail(e2e_log),
        "stderr_tail": stderr_tail,
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
    parser.add_argument("--auto-new-game", action="store_true")
    parser.add_argument("--auto-start-game", action="store_true")
    args = parser.parse_args()
    if args.game_root is None or args.timeout <= 0:
        parser.error("--game-root or GAME_ROOT and a positive --timeout are required")
    print(json.dumps(run(args.binary, args.game_root, args.library_dir,
                         args.timeout, args.crash_trace,
                         args.continue_after_frame, args.auto_new_game,
                         args.auto_start_game), indent=2))


if __name__ == "__main__":
    main()
