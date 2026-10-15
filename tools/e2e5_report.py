"""Combine E2E-5 evidence without promoting lower levels to integration PASS."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: Path) -> dict | None:
    return json.loads(path.read_text(encoding="utf-8")) if path.is_file() else None


def assemble(report_dir: Path) -> dict:
    pointer = read(report_dir / "pointer-width-audit.json")
    resources = read(report_dir / "resource-validation-full.json") or read(report_dir / "resource-validation.json")
    host = read(report_dir / "host-run.json")
    crash = read(report_dir / "crash-analysis.json")
    music = read(report_dir / "music-source-oracle.json")
    return {"schema": 1, "project_status": "SOFTWARE PARTIAL / HARDWARE PENDING",
            "static_audit": ({"status": "PARTIAL", "modules": pointer["modules_scanned"],
                              "candidates": pointer["finding_count"],
                              "unresolved": pointer["unresolved_count"]} if pointer else {"status": "NOT RUN"}),
            "resources": ({"status": resources["status"], "screens_in_config": resources.get("screen_count"),
                           "package_entries": resources.get("resource_entries"),
                           "decoded_images": len(resources.get("decoded_images", [])),
                           "oracle_unsupported": resources.get("unsupported_oracle_images")}
                          if resources else {"status": "NOT RUN"}),
            "host_methods": ([{"name": test["name"], "status": test["status"]}
                              for test in host["tests"]] if host else []),
            "integrated_original_game": host["integrated_original_program_main"] if host else {"status": "NOT RUN"},
            "crash": ({"first_game_failure": crash["first_game_failure"],
                       "unfinished_original_stage": crash.get("unfinished_original_stage"),
                       "symbolization": crash["symbolization"]} if crash else {"status": "NOT RUN"}),
            "performance": {"status": "NOT MEASURED IN E2E-5"},
            "audio_full_pipeline": {"status": "NOT RUN"},
            "installed_music_oracle": ({"status": "PASS — INDEPENDENT DECODE ONLY",
                                        "resource": music["origin"], "duration_ms": music["duration_ms"],
                                        "clipped_samples": music["clipped_samples"],
                                        "dc_offset": music["dc_offset"]} if music else {"status": "NOT RUN"}),
            "original_save_load": {"status": "NOT RUN"},
            "final_hardware_nro": {"status": "NOT BUILT — SOFTWARE GATES INCOMPLETE"}}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report-dir", type=Path, default=ROOT / "reports/e2e5")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = assemble(args.report_dir)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
