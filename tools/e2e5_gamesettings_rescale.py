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
import subprocess
import struct
import tempfile

from e2e5_resource_validator import decode_format0
from e2e5_formgameset2_compare import RESOURCE, entries, payload
from probe_gai_release import fp


def run(game_root: Path | None, library: Path, target_width: int, target_height: int,
        original_graphbuf: Path | None = None) -> dict:
    if (target_width <= 0 or target_height <= 0 or
            target_width > 8192 or target_height > 8192 or
            target_width * target_height * 4 > 128 * 1024 * 1024):
        raise ValueError("target dimensions are invalid or oversized")
    if game_root is None:
        width, height = 1920, 1200
        encoded_buffer = bytearray(96 + width * height * 2)
        encoded_buffer[:2] = b"gi"
        struct.pack_into("<i4i4I2i", encoded_buffer, 4, 1, 0, 0, width, height,
                         0xf800, 0x07e0, 0x001f, 0, 0, 1)
        struct.pack_into("<ii4i", encoded_buffer, 64, 96, width * height * 2,
                         0, 0, width, height)
        for y in range(height):
            for x in range(width):
                red, green, blue = x & 255, y & 255, (x + y) & 255
                value = ((red & 248) << 8) | ((green & 252) << 3) | (blue >> 3)
                struct.pack_into("<H", encoded_buffer, 96 + (y * width + x) * 2, value)
        encoded = bytes(encoded_buffer)
        image, decoded = decode_format0(encoded)
        resource = "SYNTHETIC:1920x1200-gradient"
    else:
        package = game_root / "DATA" / "forms.pkg"
        with package.open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as blob:
            matches = [row for row in entries(blob) if row[0].casefold() == RESOURCE]
            if len(matches) != 1:
                raise ValueError(f"expected one {RESOURCE}, found {len(matches)}")
            encoded = payload(blob, matches[0])
        image, decoded = decode_format0(encoded)
        resource = matches[0][0]
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
    # CreateFromGraphBuf(mode=1) swaps BGRA to RGBA, then asks OKGF to
    # convert the four-byte source into RGB565. Check every pixel against an
    # independent formula before treating the repack stage as covered.
    rgba = bytearray(result)
    for index in range(0, len(rgba), 4):
        rgba[index], rgba[index + 2] = rgba[index + 2], rgba[index]
    rgba_source = ctypes.create_string_buffer(bytes(rgba))
    packed_bytes = target_width * target_height * 2
    packed = ctypes.create_string_buffer(packed_bytes + guard * 2)
    ctypes.memset(packed, 0xa5, len(packed))
    convert = lib.OKGF_Convert_8888to565
    convert.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32,
                        ctypes.c_int32, ctypes.c_void_p, ctypes.c_int32,
                        ctypes.c_int32, ctypes.c_int32, ctypes.c_int32,
                        ctypes.c_int32]
    convert.restype = None
    convert(ctypes.c_void_p(ctypes.addressof(packed) + guard), target_width * 2,
            0, 0, rgba_source, target_width * 4, 0, 0,
            target_width, target_height)
    packed_result = packed.raw[guard:guard + packed_bytes]
    if packed.raw[:guard] != b"\xa5" * guard or packed.raw[-guard:] != b"\xa5" * guard:
        raise ValueError("OKGF RGB565 conversion wrote outside destination bounds")
    expected = bytearray(packed_bytes)
    for index in range(target_width * target_height):
        at = index * 4
        value = ((rgba[at] & 248) << 8) | ((rgba[at + 1] & 252) << 3) | (rgba[at + 2] >> 3)
        expected[index * 2] = value & 255
        expected[index * 2 + 1] = value >> 8
    if packed_result != expected:
        raise ValueError("OKGF RGB565 output differs from the independent pixel oracle")
    original_result = None
    if original_graphbuf is not None:
        if (target_width, target_height) != (1280, 720):
            raise ValueError("original GraphBuf fixture currently requires 1280x720")
        with tempfile.TemporaryDirectory(prefix="e2e5-graphbuf-") as directory:
            raw_path = Path(directory) / "background.bgra"
            encoded_path = Path(directory) / "background.gi"
            raw_path.write_bytes(decoded)
            encoded_path.write_bytes(encoded)
            executed = subprocess.run([str(original_graphbuf.resolve()), str(raw_path),
                                       fp(result)["fnv64"], fp(packed_result)["fnv64"],
                                       str(encoded_path), fp(decoded)["fnv64"]],
                                      capture_output=True,
                                      text=True, timeout=120, check=False)
        if executed.returncode:
            raise ValueError(f"original GraphBuf exit={executed.returncode}: "
                             f"{executed.stdout} {executed.stderr}")
        original_result = executed.stdout.strip()
    level = "SYNTHETIC" if game_root is None else "RESOURCE"
    return {"status": f"PASS — {level} GI DECODE, RESCALE AND REPACK ONLY", "resource": resource,
            "source": [source_width, source_height],
            "crop": [left, top, crop_width, crop_height],
            "target": [target_width, target_height],
            "output_bytes": output_bytes, "fnv64": fp(result)["fnv64"],
            "rgb565_bytes": packed_bytes, "rgb565_fnv64": fp(packed_result)["fnv64"],
            "original_game_executed": False,
            "original_gi_decode_executed": original_result is not None,
            "original_graphbuf_executed": original_result is not None,
            "original_gi_repack_executed": original_result is not None,
            "original_graphbuf_output": original_result}


def main() -> int:
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--game-root", type=Path)
    source.add_argument("--synthetic", action="store_true")
    parser.add_argument("--okgf-lib", required=True, type=Path)
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--original-graphbuf", type=Path)
    args = parser.parse_args()
    print(json.dumps(run(args.game_root, args.okgf_lib, args.width, args.height,
                         args.original_graphbuf),
                     ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
