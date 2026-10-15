"""Correlate E2E log's first failure with Atmosphere crash offsets.

Only symbolize against an explicitly supplied matching ELF.  A secondary
cleanup crash must never replace the first recorded game exception.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

from e2e5_scenario_runner import evaluate

BUILD = re.compile(r"\bbuild_git=([0-9a-f]{40}(?:-dirty)?)\b", re.I)
OFFSET = re.compile(r"\+\s*0x([0-9a-f]+)\)", re.I)
EXCEPTION = re.compile(r"(?:\bFAIL\b|\bexception\b|\berror\b|rejected|abort)", re.I)


def analyze(archive: Path, build_report: Path | None, elf: Path | None,
            addr2line: Path | None) -> dict:
    result = {"schema": 1, "level": "LOG AND CRASH CORRELATION",
              "archive_sha256": hashlib.sha256(archive.read_bytes()).hexdigest().upper(),
              "first_game_failure": None, "crash_reports": [], "symbolization": "NOT RUN"}
    with zipfile.ZipFile(archive) as source:
        names = source.namelist()
        log_name = next((name for name in ("e2e.log", "port.log") if name in names), None)
        lines = source.read(log_name).decode("utf-8", errors="replace").splitlines() if log_name else []
        build = BUILD.search("\n".join(lines))
        result["log_file"] = log_name
        result["build_git"] = build.group(1) if build else None
        result["observed_scenario"] = evaluate(lines)
        result["first_game_failure"] = next(({"line": index, "text": line} for index, line in enumerate(lines, 1)
                                              if (line.startswith("[E2E]") or line.startswith("[MUSIC]") or line.startswith("[AUDIO]"))
                                              and EXCEPTION.search(line)), None)
        result["last_game_checkpoint"] = next(({"line": index, "text": line} for index, line in reversed(list(enumerate(lines, 1)))
                                                if line.startswith("[E2E]") or line.startswith("[E2E3]")), None)
        original_logs = [name for name in names if name.endswith(".log") and name not in (log_name, "port.log")
                         and not name.startswith("crash_reports/")]
        result["unfinished_original_stage"] = None
        for name in original_logs:
            original_lines = source.read(name).decode("utf-8", errors="replace").splitlines()
            last = next((line.strip() for line in reversed(original_lines) if line.strip()), "")
            if last and not last.casefold().endswith("ok"):
                result["unfinished_original_stage"] = {"file": name, "text": last,
                                                         "confidence": "last log line only; exception not captured"}
        all_offsets = set()
        for name in sorted(n for n in names if n.startswith("crash_reports/") and n.endswith(".log")):
            crash_lines = source.read(name).decode("utf-8", errors="replace").splitlines()
            found = [int(match.group(1), 16) for line in crash_lines for match in OFFSET.finditer(line)]
            all_offsets.update(found)
            type_line = next((line.strip() for line in crash_lines if line.strip().startswith("Type:")), None)
            result["crash_reports"].append({"file": name, "type": type_line,
                                            "offsets": [f"0x{x:x}" for x in found[:32]],
                                            "first_offset": f"0x{found[0]:x}" if found else None})
    if build_report:
        report = json.loads(build_report.read_text(encoding="utf-8"))
        expected = report.get("build_git")
        result["build_match"] = bool(expected and result["build_git"] and
                                     result["build_git"].split("-")[0] == expected)
    if elf and addr2line:
        if build_report and not result["build_match"]:
            result["symbolization"] = "BLOCKED — BUILD ID MISMATCH"
        elif not elf.is_file() or not addr2line.is_file():
            result["symbolization"] = "BLOCKED — ELF OR ADDR2LINE MISSING"
        else:
            offsets = sorted(all_offsets)
            process = subprocess.run([str(addr2line), "-f", "-C", "-e", str(elf),
                                      *[f"0x{offset:x}" for offset in offsets]],
                                     capture_output=True, text=True)
            result["symbolization"] = "PASS" if process.returncode == 0 else "FAIL"
            if process.returncode == 0:
                symbol_lines = process.stdout.splitlines()
                result["symbols"] = {f"0x{offset:x}": symbol_lines[index * 2:index * 2 + 2]
                                     for index, offset in enumerate(offsets)}
            else:
                result["symbolization_error"] = process.stderr[-1000:]
    result["failure_signature"] = (result["first_game_failure"] or {}).get("text") or (
        result["crash_reports"][0].get("type") if result["crash_reports"] else "NO FAILURE OBSERVED")
    result["classification"] = "FIRST GAME FAILURE KNOWN" if result["first_game_failure"] else "FIRST GAME FAILURE NOT CAPTURED"
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("--build-report", type=Path)
    parser.add_argument("--elf", type=Path)
    parser.add_argument("--addr2line", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = analyze(args.archive, args.build_report, args.elf, args.addr2line)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
