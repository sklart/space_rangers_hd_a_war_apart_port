"""Run available host evidence levels and preserve unavailable integrated gates."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]


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
    args = parser.parse_args()
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
              "integrated_original_program_main": {"status": "NOT RUN", "blocker": "No host executable currently links the original complete game runtime and Switch SDL overrides"},
              "new_game_to_gameplay": {"status": "NOT RUN", "blocker": "Integrated original-game runner unavailable"},
              "save_load": {"status": "NOT RUN", "blocker": "No original-game save/load host scenario"},
              "hardware": "HARDWARE PENDING"}
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
