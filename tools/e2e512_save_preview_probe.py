"""Inspect the two real thumbnail payloads in an isolated RSG64/v168 save."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


MAX_PREVIEW_RAW = 16 * 1024 * 1024


def inspect_preview(payload: bytes, name: str) -> dict:
    decoder = zlib.decompressobj()
    raw = decoder.decompress(payload, MAX_PREVIEW_RAW + 1)
    if (len(raw) > MAX_PREVIEW_RAW or decoder.unconsumed_tail or
            not decoder.eof or decoder.unused_data):
        raise ValueError(name + " thumbnail has invalid compressed image")
    raw += decoder.flush()
    if len(raw) < 12:
        raise ValueError(name + " thumbnail image header is missing")
    width, height, stride = struct.unpack_from("<III", raw)
    if not (0 < width <= 2048 and 0 < height <= 2048 and
            width * 3 <= stride <= width * 4 and
            len(raw) == 12 + stride * height):
        raise ValueError(name + " thumbnail image dimensions are invalid")
    pixels = raw[12:]
    samples = {pixels[row * stride + col * 3:row * stride + col * 3 + 3]
               for row in range(0, height, max(1, height // 32))
               for col in range(0, width, max(1, width // 32))}
    if len(samples) < 16:
        raise ValueError(name + " thumbnail image is blank or nearly uniform")
    return {"name": name, "bytes": len(payload),
            "sha256": hashlib.sha256(payload).hexdigest(),
            "width": width, "height": height, "unique_sampled_colors": len(samples)}


def inspect(path: Path) -> dict:
    data = path.read_bytes()
    offset = 0
    fields = []
    for _ in range(8):
        begin = offset
        while offset + 2 <= len(data) and data[offset:offset + 2] != b"\0\0":
            offset += 2
        if offset + 2 > len(data) or offset - begin > 4096:
            raise ValueError("truncated save header")
        fields.append(data[begin:offset].decode("utf-16-le"))
        offset += 2
    if fields[:2] != ["RSG64", "v168"] or fields[7] != "EZ":
        raise ValueError("unexpected save signature or version")
    previews = []
    for name in ("game", "galaxy"):
        if offset + 4 > len(data):
            raise ValueError("missing preview length")
        length, = struct.unpack_from("<I", data, offset)
        offset += 4
        if not 0 < length <= 16 * 1024 * 1024 or offset + length > len(data):
            raise ValueError(name + " thumbnail empty or out of bounds")
        payload = data[offset:offset + length]
        previews.append(inspect_preview(payload, name))
        offset += length
    if previews[0]["sha256"] == previews[1]["sha256"]:
        raise ValueError("game and galaxy thumbnails are identical")
    if offset + 12 > len(data):
        raise ValueError("missing game-state payload")
    return {"status": "PASS", "save_sha256": hashlib.sha256(data).hexdigest(),
            "signature": fields[0], "version": fields[1], "previews": previews}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("save", type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.save), indent=2))
