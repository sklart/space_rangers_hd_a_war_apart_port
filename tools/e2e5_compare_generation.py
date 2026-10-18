"""Compare three isolated real-generation reports; optionally compare two builds.

No world fields are normalized away. Only elapsed_us is excluded from the
read-only key-entity checkpoint signatures. The RTL observation immediately
after construction is reported separately: native GenerateGalaxyLayout reseeds
RTL from GenerationSeed before world creation. Every completed-turn RNG value
remains in the compared signatures. The full native SaveToBuffer
payload, including native OnSave effects, is compared byte-for-byte by SHA-256.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import statistics
import sys

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / "port/switch/build/e2e5-host-game/runs"


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def inspect(report: Path) -> dict:
    raw = json.loads(report.read_text(encoding="utf-8"))
    if (raw.get("returncode") != 0 or raw.get("crashed") is not False or
            raw.get("timed_out") is not False or raw.get("stopped_by_probe") is not False or
            raw.get("generation_complete") is not True):
        raise ValueError(f"{report}: generation did not finish with its own snapshot exit")
    path = raw["stage"]
    if sys.platform == "win32" and (match := re.fullmatch(r"/mnt/([a-z])/(.*)", path)):
        path = f"{match[1]}:/{match[2]}"
    stage = Path(path).resolve(strict=True)
    if stage.parent != RUNS.resolve(strict=True):
        raise ValueError("stage is not a direct isolated probe run")
    rows_path, world = stage / "generation-checkpoints.jsonl", stage / "generation-world.bin"
    if rows_path.is_symlink() or world.is_symlink():
        raise ValueError("symlink generation output")
    rows = [json.loads(row) for row in rows_path.read_text(encoding="utf-8").splitlines()]
    expected = [(i, False) for i in range(0, 301, 25)] + [(300, True)]
    if [(r["warmup_turn"], r["final"]) for r in rows] != expected:
        raise ValueError("missing, extra or unordered completed-turn checkpoints")
    if any(b["elapsed_us"] < a["elapsed_us"] for a, b in zip(rows, rows[1:])):
        raise ValueError("nonmonotonic checkpoint elapsed time")
    log = stage / "sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log"
    if sys.platform == "win32":
        log = stage / "sdmc\uf03a/switch/space-rangers-hd-a-war-apart/logs/e2e.log"
    text = log.read_text(encoding="utf-8", errors="replace")
    fixture = re.search(r"HOST generation fixture seed=(\d+) rtl_state=(\d+) chaotic=(\d+)", text)
    saved = re.findall(r"HOST generation snapshot complete bytes=(\d+) galaxy_rng_before=(\d+) after=(\d+) rtl_rng_before=(\d+) after=(\d+) save_count_before=(\d+) after=(\d+)", text)
    if not fixture or len(saved) != 1:
        raise ValueError("missing fixture input or unique complete snapshot marker")
    size, before, after, rtl_before, rtl_after, count_before, count_after = map(int, saved[0])
    if size <= 0 or size != world.stat().st_size or count_after != count_before + 1:
        raise ValueError("world payload size or native save counter mismatch")
    seed = int(fixture[1])
    if any(r["seed"] != seed for r in rows) or before != rows[-1]["galaxy_rng"]:
        raise ValueError("seed/RNG does not match final pre-save checkpoint")
    config = {name: digest(stage / "user/config" / name)
              for name in ("CFG.TXT", "Lang.txt", "newgame.txt")}
    return {
        "report": str(report), "stage": str(stage),
        "binary_sha256": raw["binary_sha256"],
        "resource_configuration": raw["resource_configuration"],
        "config_sha256": config, "seed": seed, "rtl_after_constructor": int(fixture[2]),
        "checkpoints": [{k: v for k, v in row.items() if k != "elapsed_us"} for row in rows],
        "world_payload_sha256": digest(world), "world_payload_bytes": size,
        "native_save": {"galaxy_rng_before": before, "galaxy_rng_after": after,
                        "rtl_rng_before": rtl_before, "rtl_rng_after": rtl_after,
                        "save_count_before": count_before, "save_count_after": count_after},
        "generation_seconds": rows[-1]["elapsed_us"] / 1e6,
        "checkpoint_seconds": [r["elapsed_us"] / 1e6 for r in rows],
    }


def compare(reports: list[Path]) -> dict:
    if len(reports) != 3 or len(set(p.resolve() for p in reports)) != 3:
        raise ValueError("three distinct completed generation reports are required")
    runs = [inspect(p) for p in reports]
    if len({r["stage"] for r in runs}) != 3:
        raise ValueError("three distinct native run directories are required")
    # A repeatability group must describe one exact executable and input set.
    keys = ("binary_sha256", "resource_configuration", "config_sha256", "seed",
            "checkpoints", "world_payload_sha256", "world_payload_bytes", "native_save")
    differences = [{"run": i + 1, "fields": [key for key in keys if run[key] != runs[0][key]]}
                   for i, run in enumerate(runs[1:], 1)]
    timings = [r["generation_seconds"] for r in runs]
    return {"level": "REAL HOST GENERATION REPEATABILITY; NOT SWITCH PERFORMANCE",
            "status": "FAIL" if any(d["fields"] for d in differences) else "PASS",
            "differences": differences, "median_generation_seconds": statistics.median(timings),
            "generation_seconds": timings,
            "constructor_rtl_observation": {
                "values": [r["rtl_after_constructor"] for r in runs],
                "identical": len({r["rtl_after_constructor"] for r in runs}) == 1,
                "scope": "Before native TGalaxy::GenerateGalaxyLayout resets System::RandSeed from GenerationSeed; not a completed world checkpoint"},
            "runs": runs}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, nargs=3, required=True)
    parser.add_argument("--baseline", type=Path, nargs=3)
    args = parser.parse_args()
    try:
        candidate = compare(args.candidate)
        result = {"candidate": candidate}
        passed = candidate["status"] == "PASS"
        if args.baseline:
            baseline = compare(args.baseline)
            left, right = baseline["runs"][0], candidate["runs"][0]
            same_world = all(left[k] == right[k] for k in (
                "resource_configuration", "config_sha256", "seed",
                "checkpoints", "world_payload_sha256", "world_payload_bytes", "native_save"))
            passed = passed and baseline["status"] == "PASS" and same_world
            result.update(baseline=baseline, equivalent_world_and_rng=same_world,
                          median_time_ratio=(candidate["median_generation_seconds"] /
                                             baseline["median_generation_seconds"]))
        result["status"] = "PASS" if passed else "FAIL"
        print(json.dumps(result, indent=2))
        return 0 if passed else 1
    except (ValueError, KeyError, OSError, TypeError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "FAIL", "error": str(error)}))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
