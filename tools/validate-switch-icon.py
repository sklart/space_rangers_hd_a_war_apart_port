#!/usr/bin/env python3
"""Validate the icon embedded into the Switch NRO."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

MAX_ICON_BYTES = 128 * 1024


def fail(message: str) -> int:
    print(f"switch icon validation failed: {message}", file=sys.stderr)
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("icon", type=Path)
    icon = parser.parse_args().icon
    try:
        data = icon.read_bytes()
    except OSError as error:
        return fail(str(error))

    if len(data) > MAX_ICON_BYTES:
        return fail(f"{icon} is {len(data)} bytes; maximum is {MAX_ICON_BYTES}")
    if not data.startswith(b"\xff\xd8") or not data.endswith(b"\xff\xd9"):
        return fail(f"{icon} is not a JPEG file")

    offset = 2
    while offset < len(data):
        if data[offset] != 0xFF:
            offset += 1
            continue
        while offset < len(data) and data[offset] == 0xFF:
            offset += 1
        if offset >= len(data):
            break
        marker = data[offset]
        offset += 1
        if marker in (0xD8, 0xD9) or 0xD0 <= marker <= 0xD7 or marker == 0x01:
            continue
        if offset + 2 > len(data):
            break
        length = int.from_bytes(data[offset : offset + 2], "big")
        if length < 2 or offset + length > len(data):
            return fail(f"{icon} has a malformed JPEG marker")
        if marker in {*range(0xC0, 0xC4), *range(0xC5, 0xC8), *range(0xC9, 0xCC), *range(0xCD, 0xD0)}:
            if length < 8:
                return fail(f"{icon} has a malformed JPEG frame header")
            height = int.from_bytes(data[offset + 3 : offset + 5], "big")
            width = int.from_bytes(data[offset + 5 : offset + 7], "big")
            components = data[offset + 7]
            if (width, height) != (256, 256):
                return fail(f"{icon} is {width}x{height}; expected 256x256")
            if components != 3:
                return fail(f"{icon} has {components} components; expected RGB")
            return 0
        offset += length
    return fail(f"{icon} has no JPEG frame header")


if __name__ == "__main__":
    raise SystemExit(main())