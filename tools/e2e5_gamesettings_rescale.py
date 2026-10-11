#!/usr/bin/env python3
"""Exercise OKGF Lanczos3 on the installed GameSettings GI background.

This is a resource-backed resampler check, not execution of the original UI.
The commercial asset is read into memory and never written to the checkout.
"""

from __future__ import annotations

import argparse
import ctypes
import json
import mmap
from pathlib import Path

from e2e5_resource_validator import decode_format0
from e2e5_formgameset2_compare import RESOURCE, entries, payload
from probe_gai_release import fp


def run(game_root: Path, library: Path, target_width: int, target_height: int) -> dict:
    if (target_width <= 0 or target_height <= 0 or
            target_width > 8192 or target_height > 8192 or
            target_width * target_height * 4 > 128 * 1024 * 1024):
        raise ValueError("target dimensions are invalid or oversized")
    package = game_root / "DATA" / "forms.pkg"
    with package.open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as blob:
        matches = [row for row in entries(blob) if row[0].casefold() == RESOURCE]
        if len(matches) != 1:
            raise ValueError(f"expected one {RESOURCE}, found {len(matches)}")
        encoded = payload(blob, matches[0])
    image, decoded = decode_format0(encoded)
    source_width, source_height = image["width"], image["height"]
    # The original RescaleWithAspect uses centered CropToAspect=1 for this key.
    crop_width = source_width
    crop_height = round(target_height / target_width * source_width)
    if crop_height > source_height:
        crop_width = round(target_width / target_height * source_height)
        crop_height = source_height
    left = (source_width - crop_width) // 2
    top = (source_height - crop_height) // 2
    if min(crop_width, crop_height) < 3:
        raise ValueError("source crop is too small for Lanczos3")
    pitch = source_width * 4
    source = ctypes.create_string_buffer(decoded)
    output_bytes = target_width * target_height * 4
    guard = 32
    output = ctypes.create_string_buffer(output_bytes + guard * 2)
    ctypes.memset(output, 0xa5, len(output))
    lib = ctypes.CDLL(str(library.resolve()))
    rescale = lib.OKGF_Rescale
    rescale.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32,
                        ctypes.c_int32, ctypes.c_void_p, ctypes.c_int32,
                        ctypes.c_int32, ctypes.c_int32, ctypes.c_int32,
                        ctypes.c_int32]
    rescale.restype = None
    source_at = ctypes.c_void_p(ctypes.addressof(source) + top * pitch + left * 4)
    dest_at = ctypes.c_void_p(ctypes.addressof(output) + guard)
    rescale(dest_at, target_width, target_height, target_width * 4,
            source_at, crop_width, crop_height, pitch, 4, 5)
    result = output.raw[guard:guard + output_bytes]
    if output.raw[:guard] != b"\xa5" * guard or output.raw[-guard:] != b"\xa5" * guard:
        raise ValueError("OKGF wrote outside destination bounds")
    if result == b"\xa5" * output_bytes:
        raise ValueError("OKGF left destination unchanged")
    if not any(result) or len(set(result[:min(4096, len(result))])) < 2:
        raise ValueError("OKGF output is blank or constant")
    return {"status": "PASS — RESOURCE RESCALE ONLY", "resource": matches[0][0],
            "source": [source_width, source_height],
            "crop": [left, top, crop_width, crop_height],
            "target": [target_width, target_height],
            "output_bytes": output_bytes, "fnv64": fp(result)["fnv64"],
            "original_game_executed": False, "original_graphbuf_executed": False}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--game-root", required=True, type=Path)
    parser.add_argument("--okgf-lib", required=True, type=Path)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    args = parser.parse_args()
    print(json.dumps(run(args.game_root, args.okgf_lib, args.width, args.height),
                     ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
