#!/usr/bin/env python3
"""Independent synthetic GraphBuf bilinear oracle."""
from __future__ import annotations

import json

from probe_gai_release import fp


def bilinear(source: bytes, width: int, height: int, out_w: int, out_h: int) -> bytes:
    target = bytearray(out_w * out_h * 4)
    x_step = ((width - 1) << 16) // out_w
    y_step = ((height - 1) << 16) // out_h
    for y in range(out_h):
        yf = y * y_step
        y0, y1 = yf >> 16, min((yf >> 16) + 1, height - 1)
        wy1 = (yf & 65535) + 1
        wy0 = ((~yf) & 65535) + 1
        for x in range(out_w):
            xf = x * x_step
            x0, x1 = xf >> 16, min((xf >> 16) + 1, width - 1)
            wx1 = xf & 65535
            weights = (wy0 - ((wy0 * wx1) >> 16), (wy0 * wx1) >> 16,
                       wy1 - ((wy1 * wx1) >> 16), (wy1 * wx1) >> 16)
            for channel in range(4):
                samples = (source[(y0 * width + x0) * 4 + channel],
                           source[(y0 * width + x1) * 4 + channel],
                           source[(y1 * width + x0) * 4 + channel],
                           source[(y1 * width + x1) * 4 + channel])
                target[(y * out_w + x) * 4 + channel] = (
                    sum(sample * weight for sample, weight in zip(samples, weights)) >> 16) & 255
    return bytes(target)


if __name__ == "__main__":
    source = bytes((0, 0, 255, 255, 0, 255, 0, 255,
                    255, 0, 0, 255, 0, 0, 0, 0))
    scaled = bilinear(source, 2, 2, 4, 4)
    print(json.dumps({"synthetic_bgra_source": fp(source),
                      "bilinear_4x4": fp(scaled),
                      "bilinear_hex": scaled.hex()}, indent=2))
