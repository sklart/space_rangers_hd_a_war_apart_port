"""Evaluate observed game checkpoints from a Switch E2E log.

An absent checkpoint is NOT REACHED.  This tool never fabricates a transition.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re

STAGES = (
    ("program_main", r"(?:ProgramMain|platform runtime session log ready)"),
    ("main_menu", r"screen dispatch id=1\b"),
    ("new_game_request", r"screen_transition from=main_menu to=new_game"),
    ("game_settings_open", r"game_settings status=open"),
    ("game_settings_on_open", r"screen Run OnOpen ready"),
    ("game_settings_first_frame", r"screen Run first frame attempted"),
    ("game_settings_ok", r"game_settings status=ok|button=Ok event=up"),
    ("galaxy_generation", r"generation status=complete"),
    ("global_scripts", r"global-scripts status=complete"),
    ("introduction", r"screen_transition .*introduction|introduction status=open"),
    ("gameplay", r"gameplay status=ready"),
    ("save_load", r"roundtrip status=pass"),
)


def evaluate(lines: list[str]) -> dict:
    observed = []
    new_game_line = next((index for index, line in enumerate(lines, 1)
                          if "screen_transition from=main_menu to=new_game" in line), None)
    for name, expression in STAGES:
        match = next(({"line": index, "text": line} for index, line in enumerate(lines, 1)
                      if (name in {"program_main", "main_menu", "new_game_request"} or
                          (new_game_line is not None and index > new_game_line))
                      and re.search(expression, line, re.I)), None)
        observed.append({"stage": name, "status": "OBSERVED" if match else "NOT REACHED", "evidence": match})
    failures = [{"line": i, "text": line} for i, line in enumerate(lines, 1)
                if re.search(r"\bFAIL\b|status=(?:failed|error)|exception", line, re.I)]
    return {"schema": 1, "level": "OBSERVED GAME LOG CHECKPOINTS", "stages": observed,
            "first_failure": failures[0] if failures else None,
            "hardware": "LOG EVIDENCE ONLY; MANUAL VALIDATION PENDING"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = evaluate(args.log.read_text(encoding="utf-8", errors="replace").splitlines())
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
