"""Run available host evidence levels and preserve unavailable integrated gates."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


def summarize_original_game(probe: dict) -> dict:
    """Report only checkpoints observed by the complete original host process."""
    no_crash = not probe.get("crashed") and not probe.get("timed_out")
    startup = bool(probe.get("first_software_frame") and
                   probe.get("main_menu_on_open_ready"))
    gameplay = (startup and no_crash and probe.get("new_game") == "SCREEN DISPATCH OBSERVED" and
                probe.get("generation_complete") and probe.get("introduction_first_frame") ==
                "PRESENTATION ATTEMPT OBSERVED" and
                probe.get("dialog_choice_callback_count", 0) >= 4 and
                4 in probe.get("gameplay_screen_ids", []) and probe.get("gameplay_60_frames"))
    common = {"probe_status": probe.get("status"),
              "binary_sha256": probe.get("binary_sha256"),
              "stage": probe.get("stage"), "crashed": probe.get("crashed"),
              "screen_dispatches": probe.get("screen_dispatches", [])}
    checkpoints = {
        "main_menu": startup,
        "new_game": probe.get("new_game") == "SCREEN DISPATCH OBSERVED",
        "game_settings_saved": probe.get("settings_saved", False),
        "generation_complete": probe.get("generation_complete", False),
        "introduction": probe.get("introduction_first_frame") ==
                        "PRESENTATION ATTEMPT OBSERVED",
        "ruins_talk_four_choices": probe.get("dialog_choice_callback_count", 0) >= 4,
        "hangar_60_frames": 4 in probe.get("gameplay_screen_ids", []) and
                            probe.get("gameplay_60_frames", False),
    }
    return {
        "integrated_original_program_main": {
            **common, "status": "PASS" if startup else "FAIL",
            "main_menu_on_open_ready": probe.get("main_menu_on_open_ready", False),
            "first_software_frame": probe.get("first_software_frame")},
        "new_game_to_gameplay": {
            **common, "status": ("FAIL" if probe.get("crashed") or
                              probe.get("generation_failure_stage") is not None else
                              "TIMEOUT" if probe.get("timed_out") else
                              "PASS" if gameplay else "PARTIAL" if startup else "NOT REACHED"),
            "checkpoints": checkpoints,
            "generation_complete": probe.get("generation_complete", False),
            "dialog_choice_callback_count": probe.get("dialog_choice_callback_count", 0),
            "gameplay_60_frames": probe.get("gameplay_60_frames", False)},
        "save_load": {"status": "NOT RUN", "blocker":
                      "No original LoadGameFromFile round-trip in a second process"},
    }


def verified_probe_report(path: Path, binary: Path) -> dict:
    probe = json.loads(path.read_text(encoding="utf-8"))
    if probe.get("level") != "ORIGINAL GAME HOST PROCESS":
        raise ValueError("report is not an original-game host process probe")
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    if digest != probe.get("binary_sha256"):
        raise ValueError("host ELF hash does not match probe report")
    return probe


def run(label: str, command: list[str], timeout: int = 180) -> dict:
    start = time.monotonic()
    try:
        process = subprocess.run(command, cwd=ROOT, capture_output=True,
                                 text=True, timeout=timeout)
        return {"name": label, "status": "PASS" if process.returncode == 0 else "FAIL",
                "returncode": process.returncode, "elapsed_seconds": round(time.monotonic() - start, 3),
                "output_tail": (process.stdout + process.stderr)[-3000:]}
    except subprocess.TimeoutExpired as error:
        return {"name": label, "status": "TIMEOUT", "elapsed_seconds": round(time.monotonic() - start, 3),
                "error": str(error)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--game-root", type=Path, default=Path(os.environ["GAME_ROOT"]) if "GAME_ROOT" in os.environ else None)
    parser.add_argument("--output", type=Path, default=ROOT / "reports/e2e5/host-run.json")
    parser.add_argument("--host-binary", type=Path,
                        help="complete original-game Linux host ELF")
    parser.add_argument("--library-dir", type=Path,
                        help="directory containing the matching host libokgf.so")
    parser.add_argument("--host-report", type=Path,
                        help="existing host probe report; requires --host-binary for hash verification")
    parser.add_argument("--host-timeout", type=int, default=180)
    args = parser.parse_args()
    if args.game_root and not args.host_binary and not args.host_report and sys.platform.startswith("linux"):
        default_build = ROOT / "port/switch/build/e2e5-host-game/reproducible"
        if (default_build / "original-game-host").is_file() and (default_build / "okgf/libokgf.so").is_file():
            args.host_binary = default_build / "original-game-host"
            args.library_dir = default_build / "okgf"
    if args.host_report and not args.host_binary:
        parser.error("--host-report requires --host-binary")
    if args.host_binary and not args.host_report and (not args.game_root or not args.library_dir):
        parser.error("live host probe requires --game-root and --library-dir")
    tasks = [
        ("static pointer inventory gate", [sys.executable, "tools/e2e5_static_audit.py", "--check"]),
        ("E2E-5 tool regressions", [sys.executable, "-m", "unittest", "discover", "-s", "tests", "-p", "test_e2e5_tools.py"]),
    ]
    if args.game_root:
        tasks.append(("installed resource inventory", [sys.executable, "tools/e2e5_resource_validator.py",
                                                      "--game-root", str(args.game_root),
                                                      "--output", "reports/e2e5/resource-validation.json"]))
        tasks.append(("original New Game method fixture with installed metadata",
                      [sys.executable, "tests/test_e2e3_original_new_game.py", "--game-root", str(args.game_root)]))
    tests = [run(label, command) for label, command in tasks]
    resource_report = ROOT / "reports/e2e5/resource-validation.json"
    if args.game_root and resource_report.is_file():
        resource_status = json.loads(resource_report.read_text(encoding="utf-8"))["status"]
        for test in tests:
            if test["name"] == "installed resource inventory":
                test["resource_evidence_status"] = resource_status
                if resource_status.startswith("PARTIAL") and test["status"] == "PASS":
                    test["status"] = "PARTIAL"
    result = {"schema": 1, "level": "HOST RUNNER", "tests": tests,
              "integrated_original_program_main": {"status": "NOT RUN", "blocker": "No matching host probe requested"},
              "new_game_to_gameplay": {"status": "NOT RUN", "blocker": "No matching host probe requested"},
              "save_load": {"status": "NOT RUN", "blocker": "No original LoadGameFromFile round-trip in a second process"},
              "hardware": "HARDWARE PENDING"}
    if args.host_binary:
        try:
            if args.host_report:
                probe = verified_probe_report(args.host_report, args.host_binary)
            else:
                from e2e5_host_game_probe import run as probe_game
                probe = probe_game(args.host_binary, args.game_root, args.library_dir,
                                   args.host_timeout, None, False, mouse_new_game=True,
                                   auto_advance_dialog=True)
            result.update(summarize_original_game(probe))
            build_report_path = (ROOT / "port/switch/build/e2e5-host-game/reproducible/report.json")
            if build_report_path.is_file():
                build_report = json.loads(build_report_path.read_text(encoding="utf-8"))
                if build_report.get("binary_sha256") == probe.get("binary_sha256"):
                    result["host_build"] = {
                        "git_head": build_report.get("git_head"),
                        "git_dirty_at_build": build_report.get("git_dirty"),
                        "binary_sha256": probe.get("binary_sha256"),
                        "compiled_effective_original_units":
                            build_report.get("compiled_effective_original_units")}
            result["resource_configuration"] = probe.get("resource_configuration")
            result["tests"].append({"name": "complete original-game host process",
                                    "status": result["new_game_to_gameplay"]["status"],
                                    "binary_sha256": probe.get("binary_sha256"),
                                    "stage": probe.get("stage")})
        except (OSError, ValueError, json.JSONDecodeError) as error:
            result["tests"].append({"name": "complete original-game host process",
                                    "status": "FAIL", "error": str(error)})
    if not args.game_root:
        result["tests"].append({"name": "installed resource inventory", "status": "NOT RUN — ASSETS UNAVAILABLE"})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"tests": [{"name": x["name"], "status": x["status"]} for x in result["tests"]],
                      "integrated_original_program_main": result["integrated_original_program_main"]["status"]},
                     ensure_ascii=False, indent=2))
    return 1 if any(test["status"] in {"FAIL", "TIMEOUT"} for test in result["tests"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
