"""Independent, asset-only oracle for the first portable GAI frame decode."""
from __future__ import annotations

import argparse
import json
import pathlib
import struct
import sys
import zlib

HEADER = 48
FRAME = 8
MAX_DECODED = 256 * 1024 * 1024


def u32(data: bytes, at: int) -> int:
    return struct.unpack_from("<I", data, at)[0]


def i32(data: bytes, at: int) -> int:
    return struct.unpack_from("<i", data, at)[0]


def fnv64(data: bytes) -> str:
    value = 0xCBF29CE484222325
    for byte in data:
        value = (value ^ byte) * 0x100000001B3 & 0xFFFFFFFFFFFFFFFF
    return f"{value:016x}"


def fingerprint(data: bytes) -> dict[str, object]:
    return {"size": len(data), "crc32": f"{zlib.crc32(data):08x}", "fnv64": fnv64(data)}


def package_entries(data: bytes, folder_at: int, prefix: str = ""):
    _, count, record_size = struct.unpack_from("<III", data, folder_at)
    if record_size != 158:
        raise ValueError("unexpected package record size")
    for index in range(count):
        at = folder_at + 12 + index * record_size
        name = data[at + 71 : at + 134].split(b"\0", 1)[0].decode("ascii")
        kind = i32(data, at + 134)
        path = f"{prefix}/{name}" if prefix else name
        if kind == 3:
            yield from package_entries(data, u32(data, at + 150), path)
        else:
            yield path, kind, u32(data, at), u32(data, at + 4), u32(data, at + 150)


def package_payload(data: bytes, kind: int, size: int, position: int) -> bytes:
    at = position + 4
    if kind != 2:
        return data[at : at + size]
    output = bytearray()
    while len(output) < size:
        stored = u32(data, at)
        at += 4
        block = data[at : at + stored]
        if len(block) != stored or stored < 8 or block[:4] != b"ZL02":
            raise ValueError("invalid package ZL02 block")
        if u32(block, 4) == 0:
            raise ValueError("zero package ZL02 size")
        output.extend(zlib.decompress(block[8:]))
        at += stored
    if len(output) != size:
        raise ValueError("package payload size mismatch")
    return bytes(output)


def gai_metadata(data: bytes) -> dict[str, object]:
    if len(data) < HEADER or data[:4] != b"gai\0":
        raise ValueError("invalid GAI header")
    version = i32(data, 4)
    left, top, right, bottom = struct.unpack_from("<4i", data, 8)
    frames = i32(data, 24)
    flags = u32(data, 28)
    sequence_offset, sequence_size = struct.unpack_from("<2i", data, 32)
    if version != 1 or frames <= 0 or frames > 100000 or right <= left or bottom <= top:
        raise ValueError("invalid GAI fields")
    if HEADER + frames * FRAME > len(data):
        raise ValueError("truncated frame directory")
    sequence_count = 0
    if sequence_offset:
        if sequence_offset < 0 or sequence_size < 8 or sequence_offset + sequence_size > len(data):
            raise ValueError("invalid sequence table")
        sequence_count = i32(data, sequence_offset)
        if sequence_count < 0 or sequence_count > 100000 or 8 + sequence_count * 8 > sequence_size:
            raise ValueError("invalid sequence directory")
        for index in range(sequence_count):
            block_offset = i32(data, sequence_offset + 8 + index * 8)
            if block_offset < 0 or block_offset + 4 > sequence_size:
                raise ValueError("invalid sequence block")
            frame_count = i32(data, sequence_offset + block_offset)
            if frame_count < 0 or block_offset + 4 + frame_count * 8 > sequence_size:
                raise ValueError("invalid sequence frames")
            for frame in range(frame_count):
                source = i32(data, sequence_offset + block_offset + 4 + frame * 8)
                if source < 0 or source >= frames:
                    raise ValueError("invalid sequence source")
    elif sequence_size:
        raise ValueError("sequence size without offset")
    return {"version": version, "bounds": [left, top, right, bottom], "frame_count": frames,
            "flags": flags, "sequence_table_offset": sequence_offset, "sequence_table_size": sequence_size,
            "sequence_count": sequence_count}


def extract_frame(data: bytes, meta: dict[str, object], index: int) -> tuple[str, bytes, int, int]:
    frames = int(meta["frame_count"])
    if index < 0 or index >= frames:
        raise ValueError("invalid frame index")
    at = HEADER + index * FRAME
    offset, size = struct.unpack_from("<ii", data, at)
    if offset == 0:
        return "EMPTY", b"", offset, size
    if offset < 0 or size <= 0 or offset + size > len(data):
        raise ValueError("invalid frame range")
    stored = data[offset : offset + size]
    if stored[:2] == b"gi":
        return "RAW_GI", stored, offset, size
    if len(stored) < 8 or stored[:2] != b"ZL":
        return "UNKNOWN", b"", offset, size
    expected = u32(stored, 4)
    if not expected or expected > MAX_DECODED:
        raise ValueError("invalid compressed frame size")
    if stored[:4] not in (b"ZL01", b"ZL02"):
        return f"OTHER_{stored[:4].decode('latin1')}", b"", offset, size
    decoded = zlib.decompress(stored[8:])
    if len(decoded) != expected:
        raise ValueError("compressed frame size mismatch")
    return stored[:4].decode("ascii"), decoded, offset, size


def decode_gi_format0(data: bytes) -> tuple[dict[str, object], bytes]:
    if len(data) < 96 or data[:2] != b"gi" or i32(data, 4) != 1:
        raise ValueError("invalid embedded GI")
    left, top, right, bottom = struct.unpack_from("<4i", data, 8)
    red, green, blue, alpha = struct.unpack_from("<4I", data, 24)
    fmt, planes, clips, clip_offset = struct.unpack_from("<4i", data, 40)
    if fmt != 0 or planes < 1 or right <= left or bottom <= top or 64 + planes * 32 > len(data):
        raise ValueError("unsupported or invalid GI")
    offset, stored_size = struct.unpack_from("<2i", data, 64)
    width, height = right - left, bottom - top
    required = width * height * (4 if alpha else 2)
    if offset <= 0 or stored_size < required or offset + required > len(data):
        raise ValueError("invalid GI payload")
    if clips < 0 or (clips and (clip_offset < 0 or clip_offset + clips * 8 > len(data))):
        raise ValueError("invalid GI clips")
    source = data[offset : offset + required]
    if alpha:
        pixels = source
    else:
        pixels = bytearray(width * height * 4)
        for pixel in range(width * height):
            value = struct.unpack_from("<H", source, pixel * 2)[0]
            pixels[pixel * 4 : pixel * 4 + 4] = bytes(((value << 3) & 0xF8, (value >> 3) & 0xFC,
                                                         (value >> 8) & 0xF8, 255))
        pixels = bytes(pixels)
    return {"version": 1, "format": fmt, "bounds": [left, top, right, bottom], "planes": planes,
            "clips": clips, "masks": [f"{red:08x}", f"{green:08x}", f"{blue:08x}", f"{alpha:08x}"],
            "decoded": {"width": width, "height": height, "bpp": 4, "pitch": width * 4}}, pixels


def analyze(path: str, payload: bytes) -> dict[str, object]:
    meta = gai_metadata(payload)
    inventory = {"EMPTY": 0, "RAW_GI": 0, "ZL01": 0, "ZL02": 0, "OTHER_ZL": 0, "UNKNOWN": 0}
    formats: dict[str, int] = {str(index): 0 for index in range(7)}
    selected = None
    for index in range(int(meta["frame_count"])):
        encoding, gi, offset, stored_size = extract_frame(payload, meta, index)
        key = "OTHER_ZL" if encoding.startswith("OTHER_") else encoding
        inventory[key] = inventory.get(key, 0) + 1
        if not gi:
            continue
        try:
            header_format = i32(gi, 40) if len(gi) >= 44 and gi[:2] == b"gi" else None
            formats[str(header_format) if header_format in range(7) else "unknown"] = formats.get(str(header_format) if header_format in range(7) else "unknown", 0) + 1
            gi_meta, pixels = decode_gi_format0(gi)
        except ValueError:
            continue
        if selected is None:
            selected = {"index": index, "offset": offset, "stored_size": stored_size, "encoding": encoding,
                        "decoded_gi": fingerprint(gi), "gi": gi_meta, "pixels": fingerprint(pixels)}
    return {"resource": path, "gai": fingerprint(payload) | meta, "inventory": inventory,
            "gi_formats": formats, "selected_frame": selected}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=pathlib.Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    package = (args.game_root / "DATA" / "common.pkg").read_bytes()
    entries = sorted(package_entries(package, u32(package, 0)), key=lambda entry: entry[0].upper())
    preferred = next((entry for entry in entries if entry[0].upper() == "DATA/ASTEROID/00.GAI"), None)
    if not preferred:
        raise ValueError("missing DATA/Asteroid/00.gai")
    preferred_result = analyze(preferred[0], package_payload(package, preferred[1], preferred[3], preferred[4]))
    if preferred_result["selected_frame"] is not None:
        print(json.dumps({"asteroid_00_inventory": preferred_result, "baseline": preferred_result}, indent=2, sort_keys=True))
        return 0
    candidates = [entry for entry in entries if entry[0].upper().endswith(".GAI") and entry[0] != preferred[0]]
    for path, kind, _, size, position in candidates:
        try:
            result = analyze(path, package_payload(package, kind, size, position))
            if result["selected_frame"] is not None:
                print(json.dumps({"asteroid_00_inventory": preferred_result, "baseline": result}, indent=2, sort_keys=True))
                return 0
        except (ValueError, struct.error, zlib.error) as error:
            print(f"skip path={path} reason={error}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
