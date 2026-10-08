#!/usr/bin/env python3
"""Independent oracle for the synthetic mixed M20/M21 checkpoint scene."""
from __future__ import annotations

import struct
import zlib

FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3


def fnv64(data):
    value = FNV_OFFSET
    for byte in data:
        value = (value ^ byte) * FNV_PRIME & 0xFFFFFFFFFFFFFFFF
    return value


def fingerprint(words):
    raw = b''.join(struct.pack('<H', word) for word in words)
    return zlib.crc32(raw) & 0xffffffff, fnv64(raw), len(raw)


def checkpoint(gi_color):
    # 4x3 RGB565 background. M20 GI is a 2x2 opaque sprite at layer 0.
    pixels = [0x001F] * 12
    for y in range(2):
        for x in range(2):
            pixels[y * 4 + x] = gi_color
    # M21 Simple (blue) at layer 10.
    pixels[0] = 0x001F
    # M21 Trans at layer 20: its first pixel is key colour 0, its second green.
    pixels[5] = 0x07E0
    # M21 Alpha at layer 30: pinned OKGF's 128-alpha red over blue RGB565.
    # Its 5-bit red result is 15 (0x780f), not a rounded 16.
    pixels[2] = 0x780F
    return pixels


def main():
    frame_a = fingerprint(checkpoint(0xF800))
    frame_b = fingerprint(checkpoint(0x07E0))
    assert frame_a == (0x1A829653, 0x658AC816B5479DB3, 24), frame_a
    assert frame_b == (0x383A8729, 0x69FFB049D0071EBB, 24), frame_b
    print('M21 PRESENTATION ORACLE PASS '
          f'frame_a_crc32={frame_a[0]:08x} frame_a_fnv64={frame_a[1]:016x} '
          f'frame_b_crc32={frame_b[0]:08x} frame_b_fnv64={frame_b[1]:016x} bytes={frame_a[2]}')


if __name__ == '__main__':
    main()
