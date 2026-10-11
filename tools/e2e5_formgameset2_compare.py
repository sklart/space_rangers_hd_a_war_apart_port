#!/usr/bin/env python3
"""Compare the installed GameSettings background with the production GI decoder.

The game asset is extracted only to a temporary directory. Build
tests/test_e2e5_format0_real.cpp with gi_format0_cpu.cpp and its OKGF bridge
before running this local, release-backed check.
"""

from __future__ import annotations

import argparse
import mmap
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
from probe_ui_images_release import entries, payload  # noqa: E402
from probe_gai_release import fp  # noqa: E402
from e2e5_resource_validator import decode_format0  # noqa: E402

RESOURCE = "data/formgameset2/2bg.gi"


def compare(game_root: Path, decoder: Path) -> dict:
    package = game_root / "DATA" / "forms.pkg"
    with package.open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as blob:
        matches = [row for row in entries(blob) if row[0].casefold() == RESOURCE]
        if len(matches) != 1:
            raise ValueError(f"expected one {RESOURCE}, found {len(matches)}")
        source = payload(blob, matches[0])
    image, pixels = decode_format0(source)
    expected = fp(pixels)["fnv64"]
    with tempfile.TemporaryDirectory(prefix="e2e5-formgameset2-") as directory:
        private_path = Path(directory) / "background.gi"
        private_path.write_bytes(source)
        run = subprocess.run([str(decoder.resolve()), str(private_path),
                              str(image["width"]), str(image["height"]), expected],
                             capture_output=True, text=True, timeout=60, check=False)
    if run.returncode:
        raise ValueError(f"production decoder exit={run.returncode}: {run.stdout} {run.stderr}")
    return {"status": "PASS — RESOURCE DECODE ONLY", "resource": matches[0][0],
            "width": image["width"], "height": image["height"],
            "decoded_bytes": len(pixels), "fnv64": expected,
            "production_output": run.stdout.strip(), "original_game_executed": False}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--game-root", required=True, type=Path)
    parser.add_argument("--decoder", required=True, type=Path)
    args = parser.parse_args()
    import json
    print(json.dumps(compare(args.game_root, args.decoder), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
