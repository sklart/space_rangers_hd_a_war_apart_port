"""Summarize observable E2E-4 runtime stages without inferring hardware PASS."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


BUILD = re.compile(r"\bbuild_git=([0-9a-f]{40}(?:-dirty)?)\b", re.I)


def classify(lines: list[str]) -> dict:
    text = "\n".join(lines)
    build = BUILD.search(text)

    def status(prefix: str, proofs: tuple[str, ...]) -> str:
        relevant = [line for line in lines if line.startswith(prefix)]
        if any("FAIL" in line or "status=failed" in line or
               "status=error" in line for line in relevant):
            return "FAIL"
        if not relevant:
            return "NOT REACHED"
        if all(any(proof in line for line in relevant) for proof in proofs):
            return "PASS"
        return "NOT VERIFIED"

    result = {
        "build_git": build.group(1) if build else None,
        "hardware": "HARDWARE PENDING",
        "stages": {
            "sfx": status("[AUDIO]", ("sfx status=started", "mix status=pcm-submitted",
                                      "cursor status=advanced")),
            "music": status("[MUSIC]", ("decode status=pcm", "queue status=pcm-submitted",
                                      "cursor status=advanced")),
            "galaxy": status("[GALAXY]", ("generation status=complete",)),
            "script": status("[SCRIPT]", ("global-scripts status=complete",)),
            "save_load": status("[SAVE]", ("roundtrip status=pass",)),
            "performance": status("[PERF]", ("sample status=complete",)),
        },
    }
    if any(line.startswith("[E2E] FAIL") for line in lines):
        result["runtime"] = "FAIL"
    elif "[E2E] BOOT COMPLETE" in text:
        result["runtime"] = "PASS"
    elif any(line.startswith("[E2E]") for line in lines):
        result["runtime"] = "NOT VERIFIED"
    else:
        result["runtime"] = "NOT REACHED"
    return result


def self_test() -> None:
    empty = classify([])
    assert empty["stages"]["music"] == "NOT REACHED"
    assert empty["hardware"] == "HARDWARE PENDING"
    init_only = classify(["[E2E] build_git=" + "a" * 40,
                          "[AUDIO] backend=SDL2 status=initialized",
                          "[MUSIC] audio_init=success"])
    assert init_only["stages"]["music"] == "NOT VERIFIED"
    assert init_only["stages"]["sfx"] == "NOT VERIFIED"
    played = classify(["[MUSIC] decode status=pcm bytes=4096",
                       "[MUSIC] queue status=pcm-submitted frames=1024",
                       "[MUSIC] cursor status=advanced frames=1024"])
    assert played["stages"]["music"] == "PASS"
    failed = classify(["[MUSIC] decode status=pcm",
                       "[MUSIC] queue status=pcm-submitted",
                       "[MUSIC] cursor status=advanced",
                       "[MUSIC] FAIL decode error=-137"])
    assert failed["stages"]["music"] == "FAIL"
    assert classify(["[E2E] FAIL stage=ProgramMain"])["runtime"] == "FAIL"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", nargs="?", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        print("E2E-4 log analyzer self-test PASS")
        return 0
    if args.log is None:
        parser.error("provide an e2e.log path or --self-test")
    lines = args.log.read_text(encoding="utf-8", errors="replace").splitlines()
    print(json.dumps(classify(lines), indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
