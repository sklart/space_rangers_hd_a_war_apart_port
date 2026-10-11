"""Read-only inventory and bounded decode of installed game resources.

Private assets stay at --game-root.  The report contains names, sizes and
hashes only.  This is a resource test, not execution of the original game.
"""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import mmap
import os
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
from probe_ui_images_release import decode_dat, block_entries, data_entries, entries, payload, split_image  # noqa: E402
from probe_gai_release import decode2, gai, frame, header  # noqa: E402

GAME_ROOT_ENV = "GAME_ROOT"


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def screen_name(path: str) -> str | None:
    parts = path.split("/")
    return parts[1] if len(parts) > 1 and parts[0].casefold() == "ml" else None


def validate(root: Path, *, max_decode: int = 32) -> dict:
    report: dict = {"schema": 1, "level": "RESOURCE BACKED DATA VALIDATION",
                    "game_root": str(root), "game_executed": False,
                    "original_ui_methods_executed": False, "screens": {}, "packages": [],
                    "decoded_images": [], "errors": []}
    cfg = root / "CFG"
    data = root / "DATA"
    if not cfg.is_dir() or not data.is_dir():
        report["status"] = "NOT RUN — ASSETS UNAVAILABLE"
        return report
    config_rows = []
    cache_paths: dict[str, str] = {}
    for name, seed in (("Main.dat", None), ("CacheData.dat", 0xea8f3f37)):
        path = cfg / name
        if not path.is_file():
            report["errors"].append({"stage": "config", "file": name, "error": "missing"})
            continue
        try:
            decoded = decode_dat(path) if seed is None else decode_dat(path, seed)
            row = {"file": name, "source_bytes": path.stat().st_size,
                   "decoded_bytes": len(decoded), "decoded_sha256": sha(decoded)}
            if name == "Main.dat":
                config_rows = list(block_entries(decoded))
                row["block_values"] = len(config_rows)
            else:
                cache_records = list(data_entries(decoded))
                cache_paths = {key.casefold(): file_name.replace("\\", "/")
                               for key, file_name in cache_records}
                row["cache_entries"] = len(cache_records)
                row["unique_cache_keys"] = len(cache_paths)
            report.setdefault("configs", []).append(row)
        except (OSError, ValueError, IndexError, OverflowError) as error:
            report["errors"].append({"stage": "config", "file": name, "error": str(error)})
    screens: dict[str, dict] = {}
    for path, key, value in config_rows:
        screen = screen_name(path)
        if screen:
            row = screens.setdefault(screen, {"block_values": 0, "image_options": 0,
                                             "resource_dependencies": []})
            row["block_values"] += 1
            if key.casefold() == "image":
                row["image_options"] += 1
                mode, resource_key = split_image(value)
                cache_key = resource_key.replace(".", "/").casefold()
                package_path = cache_paths.get(cache_key)
                row["resource_dependencies"].append({"mode": mode, "option": value,
                                                      "resource_key": resource_key,
                                                      "package_path": package_path,
                                                      "resolved_in_cache": package_path is not None})
    report["screens"] = dict(sorted(screens.items()))
    report["screen_count"] = len(screens)
    report["config_values"] = len(config_rows)
    package_count = 0
    resource_count = 0
    format_counts: Counter[str] = Counter()
    package_names: set[str] = set()
    # The cap bounds decompression time; every package directory is still inventoried.
    package_priority = {"forms.pkg": 0, "mainmenu.pkg": 1, "common.pkg": 2,
                        "background.pkg": 3}
    packages = sorted(data.glob("*.pkg"), key=lambda p: (package_priority.get(p.name.casefold(), 4), p.name.casefold()))
    per_package = max(2, max_decode // max(len(packages), 1))
    for package in packages:
        package_count += 1
        try:
            with package.open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as blob:
                package_rows = list(entries(blob))
                package_names.update(name.replace("\\", "/").casefold() for name, *_ in package_rows)
                by_extension = Counter(Path(name).suffix.lower() for name, *_ in package_rows)
                resource_count += len(package_rows)
                report["packages"].append({"file": package.name, "bytes": len(blob),
                                           "entries": len(package_rows),
                                           "image_entries": sum(by_extension[ext] for ext in (".gi", ".gai", ".png", ".bmp", ".jpg", ".jpeg"))})
                selected_in_package = 0
                candidates = sorted(package_rows, key=lambda item: (
                    0 if item[0].casefold() == "data/formgameset2/2bg.gi" else
                    1 if item[0].casefold() == "data/formload2/2barcenter.gi" else 2,
                    item[0].casefold()))
                for item in candidates:
                    name, kind, size, target = item
                    extension = Path(name).suffix.lower()
                    if extension not in {".gi", ".gai"}:
                        continue
                    format_counts[extension] += 1
                    if len(report["decoded_images"]) >= max_decode or selected_in_package >= per_package:
                        continue
                    selected_in_package += 1
                    try:
                        source = payload(blob, item)
                        if not source or len(source) != size:
                            raise ValueError(f"payload size {len(source)} != {size}")
                        row = {"package": package.name, "resource": name,
                               "source_bytes": len(source), "source_sha256": sha(source)}
                        if extension == ".gi":
                            info = header(source)
                            row["format"] = info["format"]
                            if info["format"] == 2:
                                image, pixels = decode2(source)
                                row["dimensions"] = [image["decoded"]["width"], image["decoded"]["height"]]
                                row["pixels_sha256"] = sha(pixels)
                        else:
                            if not source.startswith(b"gai\x00"):
                                row["image_status"] = "NON_IMAGE_PAYLOAD_WITH_GAI_SUFFIX"
                                report["decoded_images"].append(row)
                                continue
                            info = gai(source)
                            row["frames"] = info["frame_count"]
                            if info["frame_count"]:
                                _, first, _, _ = frame(source, info, 0)
                                if first:
                                    try:
                                        first_info = header(first)
                                    except ValueError as error:
                                        row["first_frame_status"] = f"UNSUPPORTED BY ORACLE: {error}"
                                    else:
                                        row["first_frame_format"] = first_info["format"]
                                        if first_info["format"] == 2:
                                            image, pixels = decode2(first)
                                            row["first_frame_dimensions"] = [image["decoded"]["width"], image["decoded"]["height"]]
                                            row["first_frame_pixels_sha256"] = sha(pixels)
                        report["decoded_images"].append(row)
                    except (ValueError, IndexError, OverflowError, OSError, struct.error) as error:
                        report["errors"].append({"stage": "decode", "package": package.name,
                                                 "resource": name, "error": str(error)})
        except (OSError, ValueError, IndexError, OverflowError, struct.error) as error:
            report["errors"].append({"stage": "package-index", "package": package.name,
                                     "error": str(error)})
    report["package_count"] = package_count
    report["resource_entries"] = resource_count
    for screen in report["screens"].values():
        for dependency in screen["resource_dependencies"]:
            path = dependency["package_path"]
            dependency["present_in_package"] = path.replace("\\", "/").casefold() in package_names if path else False
    report["screen_image_dependencies"] = sum(screen["image_options"] for screen in report["screens"].values())
    report["screen_dependencies_resolved"] = sum(
        dependency["present_in_package"] for screen in report["screens"].values()
        for dependency in screen["resource_dependencies"])
    report["gi_gai_entries_by_format"] = dict(sorted(format_counts.items()))
    report["decode_limit"] = max_decode
    report["unsupported_oracle_images"] = sum(
        row.get("format") not in (None, 2) or "first_frame_status" in row or
        "image_status" in row or
        row.get("first_frame_format") not in (None, 2)
        for row in report["decoded_images"])
    report["status"] = ("FAIL" if report["errors"] else
                        "PARTIAL — RESOURCE DATA; SOME FORMATS UNVERIFIED" if report["unsupported_oracle_images"] else
                        "PASS — RESOURCE DATA ONLY")
    return report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--game-root", type=Path, default=Path(os.environ.get(GAME_ROOT_ENV, "")) if os.environ.get(GAME_ROOT_ENV) else None)
    parser.add_argument("--max-decode", type=int, default=32)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = validate(args.game_root, max_decode=args.max_decode) if args.game_root else {"status": "NOT RUN — ASSETS UNAVAILABLE", "level": "RESOURCE BACKED DATA VALIDATION"}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in result.items() if key not in {"screens", "packages", "decoded_images"}}, ensure_ascii=False, indent=2))
    return 1 if result["status"] == "FAIL" else 0


if __name__ == "__main__":
    raise SystemExit(main())
