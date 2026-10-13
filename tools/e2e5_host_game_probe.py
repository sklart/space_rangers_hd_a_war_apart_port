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
SCREEN_DISPATCH = re.compile(r"screen dispatch id=(\d+) postload=\d+")
SCREEN_FRAME_COUNT = re.compile(r"screen Run frames=(\d+)")
GAMEPLAY_SCREENS = frozenset((5, 6, 20))


def screen_frames(log: str) -> set[int]:
    dispatches = list(SCREEN_DISPATCH.finditer(log))
    return {int(match.group(1)) for index, match in enumerate(dispatches)
            if "screen Run first frame attempted" in log[
                match.end():dispatches[index + 1].start()
                if index + 1 < len(dispatches) else len(log)]}


def screen_frame_milestones(log: str) -> dict[int, set[int]]:
    dispatches = list(SCREEN_DISPATCH.finditer(log))
    result: dict[int, set[int]] = {}
    for index, match in enumerate(dispatches):
        section = log[match.end():dispatches[index + 1].start()
                      if index + 1 < len(dispatches) else len(log)]
        result.setdefault(int(match.group(1)), set()).update(
            int(value) for value in SCREEN_FRAME_COUNT.findall(section))
    return result


def tail(path: Path, limit: int = 5000) -> str:
    return read_log(path)[-limit:]


def read_log(path: Path) -> str:
    for _ in range(3):
        try:
            return path.read_text(encoding="utf-8", errors="replace") if path.is_file() else ""
        except OSError:
            # The game appends to this log on another thread over WSL DrvFS.
            time.sleep(0.05)
    return ""


def run(binary: Path, game_root: Path, library_dir: Path, timeout: int,
        crash_trace: Path | None, continue_after_frame: bool,
        auto_new_game: bool = False, auto_start_game: bool = False,
        auto_select_dialog: bool = False,
        mouse_new_game: bool = False) -> dict:
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
    if auto_new_game or auto_start_game or auto_select_dialog or mouse_new_game:
        env["E2E5_HOST_AUTO_NEW_GAME"] = "1"
    if mouse_new_game:
        env["E2E5_HOST_MOUSE_NEW_GAME"] = "1"
    if auto_start_game or auto_select_dialog:
        env["E2E5_HOST_AUTO_START_GAME"] = "1"
    if auto_select_dialog:
        env["E2E5_HOST_AUTO_SELECT_DIALOG"] = "1"
    if crash_trace:
        env["LD_PRELOAD"] = str(crash_trace.resolve(strict=True))
    began = time.monotonic()
    seen_frame: re.Match[str] | None = None
    seen_new_game_frame = False
    seen_gameplay_frame = False
    seen_gameplay_60_frames = False
    dialog_callback_at: float | None = None
    stopped_by_probe = False
    timed_out = False
    with stderr_path.open("w", encoding="utf-8") as stderr:
        process = subprocess.Popen([str(binary)], cwd=stage, env=env,
                                   stdout=subprocess.DEVNULL, stderr=stderr)
        while process.poll() is None and time.monotonic() - began < timeout:
            if seen_frame is None and e2e_log.is_file():
                seen_frame = FRAME.search(read_log(e2e_log))
            if (auto_new_game or auto_start_game or auto_select_dialog or mouse_new_game) and e2e_log.is_file():
                current = read_log(e2e_log)
                frames = screen_frames(current)
                seen_new_game_frame = 33 in frames
                seen_gameplay_frame = bool(frames & GAMEPLAY_SCREENS)
                milestones = screen_frame_milestones(current)
                seen_gameplay_60_frames = any(
                    60 in milestones.get(screen, set()) for screen in GAMEPLAY_SCREENS)
                if dialog_callback_at is None and "dialog choice callback complete" in current:
                    dialog_callback_at = time.monotonic()
            if ((seen_frame and not continue_after_frame and
                 not auto_new_game and not auto_start_game and not auto_select_dialog and not mouse_new_game) or
                    ((auto_new_game or mouse_new_game) and not auto_start_game and not auto_select_dialog
                     and seen_new_game_frame) or
                    (auto_start_game and not auto_select_dialog and seen_gameplay_frame
                     and seen_gameplay_60_frames) or
                    (auto_select_dialog and dialog_callback_at is not None
                     and time.monotonic() - dialog_callback_at >= 5.0)):
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
        seen_frame = FRAME.search(read_log(e2e_log))
    elapsed = round(time.monotonic() - began, 2)
    stderr_tail = tail(stderr_path)
    full_stderr = read_log(stderr_path)
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
    full_e2e_log = read_log(e2e_log)
    full_game_log = read_log(game_log)
    callback_offset = full_e2e_log.find("dialog choice callback complete")
    after_callback = full_e2e_log[callback_offset:] if callback_offset >= 0 else ""
    dispatches = [int(match.group(1)) for match in SCREEN_DISPATCH.finditer(full_e2e_log)]
    observed_frames = screen_frames(full_e2e_log)
    frame_milestones = screen_frame_milestones(full_e2e_log)
    seen_new_game_frame = 33 in observed_frames
    seen_introduction_frame = 3 in observed_frames
    generation_stages = [int(stage) for stage in re.findall(
        r"generation stage=(\d+) status=(?:running|complete)", full_e2e_log)]
    generation_failure = re.search(r"generation stage=(\d+) status=failed", full_e2e_log)
    menu_dispatch = re.search(r"screen dispatch id=1 postload=\d+", full_e2e_log)
    menu_section = (full_e2e_log[menu_dispatch.end():].split("screen dispatch id=", 1)[0]
                    if menu_dispatch else "")
    main_menu_ready = ("screen Run OnOpen begin" in menu_section and
                       "screen Run OnOpen ready" in menu_section)
    new_mouse_ids = sorted(set(re.findall(
        r"menu_action=New press_id=(\d+) result=accepted", full_e2e_log)))
    mouse_trace_pass = (len(new_mouse_ids) == 1 and
        all(full_e2e_log.count(marker.format(new_mouse_ids[0])) == 1 for marker in (
            "input press_id={} phase=dispatch message=513",
            "input press_id={} phase=dispatch message=514",
            "button=New press_id={} event=down",
            "button=New press_id={} event=up",
            "menu_action=New press_id={} result=accepted")))
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
                     "INPUT REQUESTED; NO DISPATCH" if auto_new_game or auto_start_game or auto_select_dialog or mouse_new_game
                     else "NOT RUN"),
        "auto_new_game_input": auto_new_game,
        "mouse_new_game_input": mouse_new_game,
        "mouse_new_game_press_ids": new_mouse_ids if mouse_new_game else [],
        "mouse_new_game_trace_pass": mouse_trace_pass if mouse_new_game else None,
        "auto_start_game_input": auto_start_game,
        "auto_select_dialog_input": auto_select_dialog,
        "dialog_choice_ready": "dialog choice ready" in full_e2e_log,
        "dialog_choice_callback_complete":
            callback_offset >= 0,
        "dialog_choice_ready_after_callback":
            after_callback.count("dialog choice ready") > 0,
        "dialog_60_frames_after_callback":
            "screen Run frames=60" in after_callback,
        "dialog_callback_survived_five_seconds":
            dialog_callback_at is not None and stopped_by_probe and not timed_out,
        "generation_stages": generation_stages,
        "generation_failure_stage": (int(generation_failure[1]) if generation_failure else None),
        "settings_saved": "game_settings action=apply settings=saved" in full_e2e_log,
        "generation_worker_started": "generation worker=start-requested" in full_e2e_log,
        "generation_complete": "generation stage=8 status=complete" in full_e2e_log,
        "introduction_continue_enabled":
            "introduction status=continue-enabled" in full_e2e_log,
        "introduction_first_frame": (
            "PRESENTATION ATTEMPT OBSERVED" if seen_introduction_frame else "NOT OBSERVED"),
        "gameplay_first_frame": (
            "PRESENTATION ATTEMPT OBSERVED" if observed_frames & GAMEPLAY_SCREENS
            else "NOT OBSERVED"),
        "gameplay_screen_ids": sorted(observed_frames & GAMEPLAY_SCREENS),
        "gameplay_60_frames": any(60 in frame_milestones.get(screen, set())
                                  for screen in GAMEPLAY_SCREENS),
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
    parser.add_argument("--auto-select-dialog", action="store_true")
    parser.add_argument("--mouse-new-game", action="store_true")
    args = parser.parse_args()
    if args.game_root is None or args.timeout <= 0:
        parser.error("--game-root or GAME_ROOT and a positive --timeout are required")
    print(json.dumps(run(args.binary, args.game_root, args.library_dir,
                         args.timeout, args.crash_trace,
                         args.continue_after_frame, args.auto_new_game,
                         args.auto_start_game, args.auto_select_dialog,
                         args.mouse_new_game), indent=2))


if __name__ == "__main__":
    main()
