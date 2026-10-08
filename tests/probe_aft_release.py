#!/usr/bin/env python3
"""Independent, read-only AFT structural oracle for the M13 release font keys."""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import struct
import sys
import tempfile
import zlib

from probe_ui_images_release import data_entries, decode_dat, entries, payload

LIMIT = 256 * 1024 * 1024
MAX_GLYPHS = 65536
MAX_PLANE = 16 * 1024 * 1024
MAX_DIMENSION = 8192
FONT_NAMES = re.compile(r'GlobalsV::\w+FontName\s*=\s*u"(Font\.[^"]+)"')


def fnv64(data: bytes) -> int:
    value = 0xCBF29CE484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def fingerprint(data: bytes) -> dict[str, str]:
    return {"crc32": f"{zlib.crc32(data):08x}", "fnv64": f"{fnv64(data):016x}"}


def check_rle(data: bytes, width: int, height: int, literal_bytes: int) -> None:
    if len(data) < 16:
        raise ValueError("truncated RLE header")
    stream_size, rw, rh, _ = struct.unpack_from("<IIII", data)
    if stream_size != len(data) - 16 or (rw, rh) != (width, height):
        raise ValueError("RLE header mismatch or trailing bytes")
    at = 16
    for _row in range(height):
        x = 0
        while True:
            if at == len(data):
                raise ValueError("truncated RLE row")
            command = data[at]
            at += 1
            if command == 0:
                if x != width:
                    raise ValueError("short RLE row")
                break
            if command == 128:
                if x != 0:
                    raise ValueError("invalid empty RLE row")
                break
            count = command & 127
            if not count or count > width - x:
                raise ValueError("RLE row overflow")
            if command & 128:
                size = count * literal_bytes
                if size > len(data) - at:
                    raise ValueError("truncated RLE literal")
                at += size
            x += count
    if at != len(data):
        raise ValueError("RLE trailing bytes or wrong row count")


def plane(source: bytes, at: int, literal_bytes: int) -> tuple[tuple[int, ...], int]:
    left, top, width, height, offset, size = struct.unpack_from("<iiiiii", source, at)
    if min(width, height) < 0 or max(width, height) > MAX_DIMENSION:
        raise ValueError("invalid plane dimensions")
    if not -(2**31) <= left + width <= 2**31 - 1 or not -(2**31) <= top + height <= 2**31 - 1:
        raise ValueError("plane coordinate overflow")
    if offset == 0:
        if size:
            raise ValueError("absent plane has data size")
        return (left, top, width, height, offset, size), 0
    if offset < 0 or size < 16 or size > MAX_PLANE or width == 0 or height == 0 or offset > len(source) or size > len(source) - offset:
        raise ValueError("invalid plane data range")
    check_rle(source[offset:offset + size], width, height, literal_bytes)
    return (left, top, width, height, offset, size), size


def aft(source: bytes) -> dict:
    if not 32 <= len(source) <= LIMIT:
        raise ValueError("invalid AFT source size")
    if source[:3] != b"aft":
        raise ValueError("invalid AFT magic")
    version, count, centering = struct.unpack_from("<iii", source, 4)
    stored_line = struct.unpack_from("<i", source, 20)[0]
    if version != 1 or count < 0 or count > MAX_GLYPHS or count > (len(source) - 32) // 64:
        raise ValueError("invalid AFT version or glyph table")
    if stored_line > 2**31 - 3:
        raise ValueError("AFT line height overflow")
    lookup = {}
    above = below = maximum = duplicates = largest_plane = 0
    glyph_bytes = bytearray()
    for index in range(count):
        at = 32 + index * 64
        code, a, b, c = struct.unpack_from("<Iiii", source, at)
        if code > 0xFFFF:
            raise ValueError("non-BMP glyph code")
        advance = a + b + c
        if not -(2**31) <= advance <= 2**31 - 1:
            raise ValueError("glyph advance overflow")
        maximum = max(maximum, advance)
        opaque, opaque_size = plane(source, at + 16, 0)
        alpha, alpha_size = plane(source, at + 40, 1)
        largest_plane = max(largest_plane, opaque_size, alpha_size)
        for p in (opaque, alpha):
            if p[4]:
                if p[1] <= 0:
                    above = max(above, -p[1] + 1)
                below = max(below, p[1] + p[3] - 1)
        if code in lookup:
            duplicates += 1
        lookup[code] = index  # Upstream lookup stores the last duplicate.
        glyph_bytes += source[at:at + 64]
    canonical = (struct.pack("<IIIIIII", version, count, centering & 0xFFFFFFFF,
                             (stored_line + 2) & 0xFFFFFFFF, above, below, maximum) + glyph_bytes)
    codes = set(lookup)
    return {
        "glyph_count": count, "duplicate_codepoints": duplicates,
        "line_height": stored_line + 2, "centering_height": centering,
        "above_baseline": above, "below_baseline": below, "max_advance": maximum,
        "largest_plane": largest_plane,
        "min_char_code": min(codes) if codes else None,
        "max_char_code": max(codes) if codes else None,
        "ascii_coverage": sum(32 <= c <= 126 for c in codes),
        "cyrillic_coverage": sum(0x400 <= c <= 0x52F for c in codes),
        "space_present": 32 in codes, "question_present": 63 in codes,
        "structural": fingerprint(canonical),
    }


def release_keys(repository: pathlib.Path) -> list[str]:
    text = (repository / "port/switch/platform/ui_metadata_slice.cpp").read_text(encoding="utf-8")
    keys = FONT_NAMES.findall(text)
    if len(keys) != 17 or len(set(keys)) != 17:
        raise ValueError("M13 metadata does not define exactly 17 distinct font keys")
    return keys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=pathlib.Path)
    parser.add_argument("--json", type=pathlib.Path)
    parser.add_argument("--cpp-binary", type=pathlib.Path)
    args = parser.parse_args()
    repository = pathlib.Path(__file__).resolve().parents[1]
    keys = release_keys(repository)
    cache = args.game_root / "CFG/CacheData.dat"
    paths = {key.replace("/", ".").casefold(): value.replace("\\", "/")
             for key, value in data_entries(decode_dat(cache, 0xEA8F3F37))}
    requested = {}
    for key in keys:
        if key.casefold() not in paths:
            raise ValueError(f"unresolved font key {key}")
        requested.setdefault(paths[key.casefold()].casefold(), []).append(key)
    found = {}
    with tempfile.TemporaryDirectory(prefix="m23-aft-") as temp:
        extracted = {}
        for package in sorted((args.game_root / "DATA").glob("*.pkg")):
            blob = package.read_bytes()
            for entry in entries(blob):
                path = entry[0].replace("\\", "/").casefold()
                if path not in requested or all(key in found for key in requested[path]):
                    continue
                source = payload(blob, entry)
                detail = aft(source)
                for key in requested[path]:
                    found[key] = {"key": key, "resource": paths[key.casefold()],
                                  "package": package.name, "source_size": len(source),
                                  "source": fingerprint(source), **detail, "validation": "PASS"}
                    if args.cpp_binary:
                        target = pathlib.Path(temp) / (key.replace(".", "_") + ".aft")
                        target.write_bytes(source)
                        extracted[str(target)] = key
            if len(found) == len(keys):
                break
        if args.cpp_binary:
            if len(found) != len(keys):
                raise ValueError("cannot compare C++ parser before all fonts resolve")
            completed = subprocess.run([str(args.cpp_binary.resolve()), *extracted],
                                       capture_output=True, text=True, check=True)
            if len(completed.stdout.splitlines()) != len(keys):
                raise ValueError("C++ font output count mismatch")
            for line in completed.stdout.splitlines():
                values = line.split("|")
                if len(values) != 10:
                    raise ValueError("malformed C++ font output")
                key = extracted[values[0]]
                font = found[key]
                expected = [font["glyph_count"], font["duplicate_codepoints"],
                            font["line_height"], font["centering_height"],
                            font["above_baseline"], font["below_baseline"], font["max_advance"]]
                if [int(value) for value in values[1:8]] != expected or values[8] != font["structural"]["crc32"] or values[9] != font["structural"]["fnv64"]:
                    raise ValueError(f"C++/Python AFT mismatch: {key}")
    if len(found) != len(keys):
        raise ValueError(f"missing release fonts: {sorted(set(keys) - found.keys())}")
    result = {"fonts": [found[key] for key in keys],
              "total_glyphs": sum(font["glyph_count"] for font in found.values()),
              "duplicate_codepoints": sum(font["duplicate_codepoints"] for font in found.values())}
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    else:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, struct.error, subprocess.CalledProcessError) as exc:
        print(f"AFT release probe FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
