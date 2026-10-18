"""Run an already built E2E-5 host ELF against installed assets in isolation.

This probe reports observed startup, screen, and software-frame checkpoints.
Its optional New Game input uses the original menu's Return-key handler.
"""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
from uuid import uuid4


ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / "port/switch/build/e2e5-host-game/runs"
FRAME = re.compile(
    r"HOST first software frame width=(\d+) height=(\d+) rgb565_hash=([0-9a-f]{16})"
)
SCREEN_DISPATCH = re.compile(r"screen dispatch id=(\d+) postload=\d+")
SCREEN_FRAME_COUNT = re.compile(r"screen Run(?:Continuous)? frames=(\d+)")
GAMEPLAY_SCREENS = frozenset((4, 5, 6, 20))
POST_DIALOG_SCREENS = frozenset((4, 5, 6))


def screen_frames(log: str) -> set[int]:
    dispatches = list(SCREEN_DISPATCH.finditer(log))
    return {int(match.group(1)) for index, match in enumerate(dispatches)
            if ("screen Run first frame attempted" in log[
                match.end():dispatches[index + 1].start()
                if index + 1 < len(dispatches) else len(log)] or
                "screen RunContinuous frames=1" in log[
                match.end():dispatches[index + 1].start()
                if index + 1 < len(dispatches) else len(log)])}


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


def hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_save_inventory(log: str) -> dict | None:
    entries = []
    summary = None
    for line in log.splitlines():
        if line.startswith("E2E5_SAVE_INV|"):
            fields = line.split("|")
            if len(fields) == 8:
                entries.append({"script": fields[1], "scope": fields[2],
                                "variable": fields[3], "value_kind": fields[4],
                                "category": fields[5], "object_type": fields[6],
                                "object_id": int(fields[7])})
        elif line.startswith("E2E5_SAVE_INV_SUMMARY|"):
            fields = line.split("|")
            if len(fields) == 3:
                summary = {"inspected": int(fields[1]), "unknown": int(fields[2])}
    if summary is None:
        return None
    counts = Counter(entry["category"] for entry in entries)
    complete = len(entries) == summary["inspected"]
    return {"schema": 1, "status": "PASS" if summary["unknown"] == 0 and complete else "BLOCKED",
            "inspected": summary["inspected"], "unknown": summary["unknown"],
            "entries_complete": complete,
            "category_counts": dict(sorted(counts.items())),
            "entries": entries,
            "limitations": ["Object identity is checked at the save checkpoint; producer semantics for numeric values still require review.",
                            "TScriptPlace::TargetValue is reconstructed from script definitions and is not a direct disk field."]}


def run(binary: Path, game_root: Path, library_dir: Path, timeout: int,
        crash_trace: Path | None, continue_after_frame: bool,
        auto_new_game: bool = False, auto_start_game: bool = False,
        auto_select_dialog: bool = False,
        mouse_new_game: bool = False, capture_audio: bool = False,
        auto_advance_dialog: bool = False,
        auto_quicksave: bool = False,
        load_save: Path | None = None,
        auto_options: bool = False, cfg_file: Path | None = None,
        lang_file: Path | None = None, auto_takeoff: bool = False,
        new_game_file: Path | None = None) -> dict:
    binary = binary.resolve(strict=True)
    game_root = game_root.resolve(strict=True)
    library_dir = library_dir.resolve(strict=True)
    if not binary.is_file() or not game_root.is_dir() or not library_dir.is_dir():
        raise ValueError("binary, game root, and library directory must exist")
    RUNS.mkdir(parents=True, exist_ok=True)
    stage = RUNS / uuid4().hex
    stage.mkdir()
    cfg_input_sha256 = None
    if cfg_file is not None:
        cfg_source = cfg_file.resolve(strict=True)
        cfg_target = stage / "user/config/CFG.TXT"
        cfg_target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(cfg_source, cfg_target)
        cfg_input_sha256 = hash_file(cfg_target)
    if lang_file is not None:
        lang_source = lang_file.resolve(strict=True)
        lang_target = stage / "user/config/Lang.txt"
        lang_target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(lang_source, lang_target)
    new_game_input_sha256 = None
    if new_game_file is not None:
        new_game_source = new_game_file.resolve(strict=True)
        new_game_target = stage / "user/config/newgame.txt"
        new_game_target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(new_game_source, new_game_target)
        new_game_input_sha256 = hash_file(new_game_target)
    game_log = stage / "user/config/########.log"
    e2e_log = stage / "sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log"
    stderr_path = stage / "stderr.txt"
    audio_path = stage / "audio-output.raw"
    quicksave_path = stage / "user/config/save/QuickSave.sav"
    load_source_sha256 = None
    if load_save is not None:
        source = load_save.resolve(strict=True)
        if (load_save.is_symlink() or source.name != "QuickSave.sav" or
                not source.is_file() or RUNS.resolve() not in source.parents):
            raise ValueError("--load-save must be an isolated probe QuickSave.sav")
        quicksave_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, quicksave_path)
        load_source_sha256 = hash_file(source)
        if hash_file(quicksave_path) != load_source_sha256:
            raise RuntimeError("isolated save copy hash mismatch")
    env = os.environ.copy()
    env.update(E2E5_GAME_ROOT=str(game_root), SDL_VIDEODRIVER="dummy",
               SDL_AUDIODRIVER="dummy", LD_LIBRARY_PATH=str(library_dir))
    if capture_audio:
        env["SDL_AUDIODRIVER"] = "disk"
        env["SDL_DISKAUDIOFILE"] = str(audio_path)
        env["SDL_DISKAUDIODELAY"] = "11"
    if auto_new_game or auto_start_game or auto_select_dialog or mouse_new_game or auto_advance_dialog or auto_quicksave or auto_takeoff:
        env["E2E5_HOST_AUTO_NEW_GAME"] = "1"
    if mouse_new_game:
        env["E2E5_HOST_MOUSE_NEW_GAME"] = "1"
    if auto_start_game or auto_select_dialog or auto_advance_dialog or auto_quicksave or auto_takeoff:
        env["E2E5_HOST_AUTO_START_GAME"] = "1"
    if auto_select_dialog:
        env["E2E5_HOST_AUTO_SELECT_DIALOG"] = "1"
    if auto_advance_dialog:
        env["E2E5_HOST_AUTO_ADVANCE_DIALOG"] = "1"
    if auto_quicksave:
        env["E2E5_HOST_AUTO_QUICKSAVE"] = "1"
    if auto_takeoff:
        env["E2E5_HOST_AUTO_SELECT_DIALOG"] = "1"
        env["E2E5_HOST_AUTO_ADVANCE_DIALOG"] = "1"
        env["E2E5_HOST_AUTO_TAKEOFF"] = "1"
    if load_save is not None:
        env["E2E5_HOST_LOAD_SAVE"] = "1"
    if auto_options:
        env["E2E5_HOST_AUTO_OPTIONS"] = "1"
    if crash_trace:
        env["LD_PRELOAD"] = str(crash_trace.resolve(strict=True))
    began = time.monotonic()
    seen_frame: re.Match[str] | None = None
    seen_new_game_frame = False
    seen_gameplay_frame = False
    seen_gameplay_60_frames = False
    dialog_callback_at: float | None = None
    quicksave_at: float | None = None
    quicksave_size = 0
    quicksave_rejected = False
    load_rejected = False
    turn_complete_at: float | None = None
    frames: set[int] = set()
    milestones: dict[int, set[int]] = {}
    stopped_by_probe = False
    timed_out = False
    with stderr_path.open("w", encoding="utf-8") as stderr:
        process = subprocess.Popen([str(binary)], cwd=stage, env=env,
                                   stdout=subprocess.DEVNULL, stderr=stderr)
        while process.poll() is None and time.monotonic() - began < timeout:
            current = ""
            if seen_frame is None and e2e_log.is_file():
                seen_frame = FRAME.search(read_log(e2e_log))
            if (auto_new_game or auto_start_game or auto_select_dialog or mouse_new_game or
                    auto_advance_dialog or auto_quicksave or auto_takeoff or load_save is not None or
                    auto_options or os.environ.get("E2E5_HOST_MENU_PROBE")) and e2e_log.is_file():
                current = read_log(e2e_log)
                frames = screen_frames(current)
                seen_new_game_frame = 33 in frames
                seen_gameplay_frame = bool(frames & GAMEPLAY_SCREENS)
                milestones = screen_frame_milestones(current)
                seen_gameplay_60_frames = any(
                    60 in milestones.get(screen, set()) for screen in GAMEPLAY_SCREENS)
                if dialog_callback_at is None and "dialog choice callback complete" in current:
                    dialog_callback_at = time.monotonic()
                if (load_save is not None and turn_complete_at is None and
                        "hangar_end_turn_done turn=" in current):
                    turn_complete_at = time.monotonic()
            if auto_quicksave and quicksave_path.is_file():
                size = quicksave_path.stat().st_size
                if size > 0 and size != quicksave_size:
                    quicksave_size = size
                    quicksave_at = time.monotonic()
            if auto_quicksave and ("E2E4 script pointer cannot be serialized" in read_log(game_log) or
                                   "E2E5 save inventory contains unknown" in read_log(game_log) or
                                   "E2E4 quicksave rotation failed" in read_log(game_log)):
                quicksave_rejected = True
            if load_save is not None and "Exception E2E5 script object ID has no loaded object" in read_log(game_log):
                load_rejected = True
            turns_target = max(0, int(os.environ.get("E2E59_HOST_END_TURNS", "0")))
            turns_complete = current.count("turn operation=end-turn-ui")
            if auto_takeoff:
                turns_complete = max(0, current.count("HOST map orders ready") - 1)
            if ((turns_target and turns_complete >= turns_target) or (not turns_target and ((seen_frame and not continue_after_frame and
                 not auto_new_game and not auto_start_game and not auto_select_dialog and
                 not auto_advance_dialog and
                 not auto_quicksave and not mouse_new_game and not capture_audio and
                 not auto_options and not auto_takeoff and
                 load_save is None) or
                    (os.environ.get("E2E5_HOST_MENU_PROBE") and
                     "HOST menu probe completed" in current) or
                    ((auto_new_game or mouse_new_game) and not auto_start_game and
                     not auto_select_dialog and not auto_advance_dialog and not auto_quicksave
                     and seen_new_game_frame) or
                    (auto_start_game and not auto_select_dialog and not auto_advance_dialog and
                     not auto_quicksave and
                     seen_gameplay_frame
                     and seen_gameplay_60_frames) or
                    (auto_select_dialog and dialog_callback_at is not None
                     and not auto_advance_dialog and not auto_quicksave and
                     time.monotonic() - dialog_callback_at >= 5.0) or
                    (auto_advance_dialog and not auto_quicksave and not auto_takeoff and
                     bool(frames & POST_DIALOG_SCREENS) and
                     any(60 in milestones.get(screen, set())
                         for screen in POST_DIALOG_SCREENS)) or
                    (auto_takeoff and 16 in frames and
                     60 in milestones.get(16, set())) or
                    (auto_options and current.count("screen dispatch id=26 postload=") >= 2 and
                     current.count("options groups constructed") >= 2 and
                     "options active group=5" in current and
                     "options choice selected VSync=" in current and
                     current.count("screen dispatch id=1 postload=") >= 3 and
                     current.rfind("screen Run OnOpen ready") >
                         current.rfind("screen dispatch id=1 postload=")) or
                    (auto_quicksave and quicksave_rejected) or
                    (load_save is not None and load_rejected) or
                    (load_save is not None and turn_complete_at is not None and
                     time.monotonic() - turn_complete_at >= 5.0) or
                    (auto_quicksave and quicksave_at is not None and
                     not (quicksave_path.parent / "save.tmp").exists() and
                     time.monotonic() - quicksave_at >= 2.0)))):
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
    save_inventory = parse_save_inventory(full_stderr)
    game_states = [{"phase": phase, "turn": int(turn), "money": int(money),
                    "player_ship_id": int(ship), "star_id": int(star),
                    "planet_id": int(planet), "script_count": int(scripts)}
                   for phase, turn, money, ship, star, planet, scripts in re.findall(
                       r"E2E5_STATE\|(save|load)\|(\d+)\|(-?\d+)\|(\d+)\|(\d+)\|(\d+)\|(\d+)",
                       full_stderr)]
    crashed = not stopped_by_probe and (
        (process.returncode is not None and process.returncode < 0) or
        "E2E5 HOST SIGNAL" in full_stderr)
    if load_rejected and not game_states:
        state = "LOAD_REJECTED"
    elif crashed:
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
    end_turns = [int(value) for value in re.findall(
        r"hangar_end_turn_done turn=(\d+)", full_e2e_log)]
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
    audio_capture = None
    if capture_audio:
        spec = re.search(
            r"E2E AUDIO obtained freq=(\d+) format=(\d+) channels=(\d+) "
            r"samples=(\d+) driver=(\w+)", full_stderr)
        audio_capture = {"status": "CAPTURE UNAVAILABLE", "path": str(audio_path)}
        if spec and spec[5] == "disk" and int(spec[3]) == 2 and audio_path.is_file():
            size = audio_path.stat().st_size
            if 0 < size <= 16 * 1024 * 1024 and size % 4 == 0:
                from e2e5_audio_capture import analyze_pcm
                raw = audio_path.read_bytes()
                audio_capture = analyze_pcm(raw, 2, int(spec[1]),
                    "original ProgramMain through SDL2 disk output",
                    "ORIGINAL GAME HOST SDL OUTPUT")
                audio_capture["path"] = str(audio_path)
                audio_capture["sha256"] = hashlib.sha256(raw).hexdigest().upper()
                audio_capture["driver"] = spec[5]
                audio_capture["sdl_format"] = int(spec[2])
                audio_capture["original_game_event_verified"] = (
                    "[AUDIO] music status=decoding" in full_game_log and
                    max((row[0] for row in ring_rows), default=0) > 0)
                audio_capture["status"] = "CAPTURED" if audio_capture["peak"] > 0 else "SILENT"
            else:
                audio_capture["error"] = f"raw size {size} outside bounded stereo S16 capture"
    result = {
        "schema": 1,
        "level": "ORIGINAL GAME HOST PROCESS",
        "effective_source_scope": "UNVERIFIED BY THIS PROBE",
        "resource_configuration": {
            "game_root": str(game_root),
            "cfg_input_sha256": cfg_input_sha256,
            "new_game_input_sha256": new_game_input_sha256,
            "main_dat_sha256": hash_file(game_root / "CFG/Main.dat")
            if (game_root / "CFG/Main.dat").is_file() else None,
            "cache_data_dat_sha256": hash_file(game_root / "CFG/CacheData.dat")
            if (game_root / "CFG/CacheData.dat").is_file() else None,
        },
        "status": state,
        "menu_probe_completed": "HOST menu probe completed" in full_e2e_log,
        "arcade_60_frames": "screen RunContinuous frames=60" in full_e2e_log[
            full_e2e_log.find("screen dispatch id=21 "):] if "screen dispatch id=21 " in full_e2e_log else False,
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
                     "INPUT REQUESTED; NO DISPATCH" if auto_new_game or auto_start_game or auto_select_dialog or mouse_new_game or auto_advance_dialog or auto_quicksave or auto_takeoff
                     else "NOT RUN"),
        "auto_new_game_input": auto_new_game,
        "mouse_new_game_input": mouse_new_game,
        "mouse_new_game_press_ids": new_mouse_ids if mouse_new_game else [],
        "mouse_new_game_trace_pass": mouse_trace_pass if mouse_new_game else None,
        "auto_start_game_input": auto_start_game or auto_takeoff,
        "auto_select_dialog_input": auto_select_dialog or auto_takeoff,
        "auto_advance_dialog_input": auto_advance_dialog or auto_takeoff,
        "auto_quicksave_input": auto_quicksave,
        "auto_takeoff_input": auto_takeoff,
        "requested_end_turns": int(os.environ.get("E2E59_HOST_END_TURNS", "0")),
        "native_map_swipe_order_preserved": "HOST native map swipe moved=1 ship_order_preserved=1" in full_e2e_log,
        "native_map_mode_cancel_pass": "HOST native canceled gesture released without tap PASS" in full_e2e_log,
        "observed_end_turns": (max(0, full_e2e_log.count("HOST map orders ready") - 1)
                               if auto_takeoff else full_e2e_log.count("turn operation=end-turn-ui")),
        "turn_timings": [{"operation": op, "screen": int(screen), "elapsed_us": int(us)}
                         for op, screen, us in re.findall(
                             r"turn operation=([a-z-]+) screen=(\d+) elapsed_us=(\d+)", full_e2e_log)],
        "takeoff_loading_dispatched": "screen dispatch id=22 postload=16" in full_e2e_log,
        "post_takeoff_screen_60_frames": 60 in frame_milestones.get(16, set()),
        "auto_options_input": auto_options,
        "options_open_count": full_e2e_log.count("screen dispatch id=26 postload="),
        "options_groups_constructed_count": full_e2e_log.count("options groups constructed"),
        "options_active_groups": [int(index) for index in
            re.findall(r"options active group=(\d+)", full_e2e_log)],
        "options_choice_selected": "options choice selected VSync=" in full_e2e_log,
        "load_save_input": load_save is not None,
        "load_source_sha256": load_source_sha256,
        "game_states": game_states,
        "load_screen_dispatched": 24 in dispatches,
        "loaded_hangar_60_frames": 24 in dispatches and 60 in frame_milestones.get(4, set()),
        "post_load_ship_open": 24 in dispatches and "ship_equipment_open" in full_e2e_log,
        "post_load_ship_60_frames": 24 in dispatches and
            "ship_equipment_open" in full_e2e_log and
            "screen Run frames=60" in full_e2e_log[full_e2e_log.find("ship_equipment_open"):],
        "post_load_ship_closed": "ship_equipment_closed" in full_e2e_log,
        "post_load_end_turn": end_turns[-1] if end_turns else None,
        "post_load_turn_advanced": bool(end_turns and game_states and
            end_turns[-1] > game_states[-1]["turn"]),
        "quicksave_serialization_rejected":
            ("E2E4 script pointer cannot be serialized" in full_game_log or
             "E2E5 save inventory contains unknown" in full_game_log),
        "load_rejected": load_rejected,
        "save_inventory": {"status": save_inventory["status"],
                           "inspected": save_inventory["inspected"],
                           "unknown": save_inventory["unknown"]}
                          if save_inventory else None,
        "quicksave": ({"path": str(quicksave_path), "size": quicksave_path.stat().st_size,
                       "sha256": hash_file(quicksave_path)}
                      if quicksave_path.is_file() else None),
        "dialog_choice_callback_count": full_e2e_log.count("dialog choice callback complete"),
        "post_dialog_screen_ids": sorted(observed_frames & POST_DIALOG_SCREENS),
        "dialog_choice_ready": "dialog choice ready" in full_e2e_log,
        "dialog_choice_callback_complete":
            callback_offset >= 0,
        "dialog_choice_ready_after_callback":
            after_callback.count("dialog choice ready") > 0,
        "dialog_60_frames_after_callback":
            "screen Run frames=60" in after_callback,
        "dialog_callback_survived_five_seconds":
            dialog_callback_at is not None and not crashed and
            time.monotonic() - dialog_callback_at >= 5.0,
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
            "pcm_output_verified": bool(audio_capture and
                audio_capture.get("status") == "CAPTURED" and
                audio_capture.get("original_game_event_verified")),
            "capture": audio_capture,
        },
        "stage": str(stage),
        "game_log_tail": tail(game_log),
        "e2e_log_tail": tail(e2e_log),
        "stderr_tail": stderr_tail,
    }
    (stage / "report.json").write_text(json.dumps(result, indent=2) + "\n",
                                       encoding="utf-8")
    if auto_quicksave and save_inventory:
        save_inventory.update(binary_sha256=result["binary_sha256"],
                              resource_configuration=result["resource_configuration"],
                              stage=str(stage))
        build_report = ROOT / "port/switch/build/e2e5-host-game/reproducible/report.json"
        if build_report.is_file():
            provenance = json.loads(build_report.read_text(encoding="utf-8"))
            if provenance.get("binary_sha256") == result["binary_sha256"]:
                save_inventory["source_git_sha"] = provenance.get("git_head")
                save_inventory["source_dirty_at_build"] = provenance.get("git_dirty")
        output = ROOT / "reports/e2e5/save-pointer-inventory.json"
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(save_inventory, indent=2) + "\n", encoding="utf-8")
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
    parser.add_argument("--auto-advance-dialog", action="store_true")
    parser.add_argument("--auto-quicksave", action="store_true")
    parser.add_argument("--load-save", type=Path,
                        help="QuickSave.sav from an earlier isolated probe stage")
    parser.add_argument("--mouse-new-game", action="store_true")
    parser.add_argument("--capture-audio", action="store_true")
    parser.add_argument("--auto-options", action="store_true")
    parser.add_argument("--auto-takeoff", action="store_true")
    parser.add_argument("--cfg-file", type=Path,
                        help="copy this CFG.TXT into the isolated user directory")
    parser.add_argument("--new-game-file", type=Path,
                        help="copy native newgame.txt settings into the isolated user directory")
    parser.add_argument("--lang-file", type=Path,
                        help="copy this Lang.txt into the isolated user directory")
    args = parser.parse_args()
    if args.game_root is None or args.timeout <= 0:
        parser.error("--game-root or GAME_ROOT and a positive --timeout are required")
    if args.capture_audio and args.timeout > 80:
        parser.error("--capture-audio requires --timeout at most 80 seconds")
    print(json.dumps(run(args.binary, args.game_root, args.library_dir,
                         args.timeout, args.crash_trace,
                         args.continue_after_frame, args.auto_new_game,
                         args.auto_start_game, args.auto_select_dialog,
                         args.mouse_new_game, args.capture_audio,
                         args.auto_advance_dialog, args.auto_quicksave,
                         args.load_save, args.auto_options, args.cfg_file,
                         args.lang_file, args.auto_takeoff, args.new_game_file), indent=2))


if __name__ == "__main__":
    main()
