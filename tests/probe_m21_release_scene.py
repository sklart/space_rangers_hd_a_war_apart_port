#!/usr/bin/env python3
"""Independent oracle for M21's fixed first runtime presentation frame.

This deliberately reads package bytes and decodes the configured Simple PNG and
the M20 GAI resources without using any portable C++ production output.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__.replace("\\", "/")).resolve().parent
sys.path.insert(0, str(ROOT))
from probe_gai_release import decode2, entries, frame, gai, payload, u32
from probe_m21_simple_oracle import decode_indexed_png, fnv64, package_entries, package_payload

WIDTH, HEIGHT, CLEAR = 1280, 720, 0x0010


def pack565(red, green, blue):
    return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)


def unpack565(value):
    # Keep this byte expansion identical to software_compositor::Unpack565.
    # The 255-based scale, rather than a left shift, affects the later alpha
    # blend of the M20 overlay and therefore the release-frame fingerprint.
    return ((value >> 11) & 31) * 255 // 31, ((value >> 5) & 63) * 255 // 63, (value & 31) * 255 // 31


def blend(source, destination, alpha):
    return (source * alpha + destination * (255 - alpha) + 127) // 255


def decode_gai(source):
    container = gai(source)
    _, gi, _, _ = frame(source, container, 0)
    if not gi:
        raise ValueError("empty GAI frame")
    metadata, pixels = decode2(gi)
    decoded = metadata["decoded"]
    return decoded["width"], decoded["height"], decoded["pitch"], pixels


def draw_bgra(framebuffer, image, x, y, object_alpha):
    width, height, pitch, pixels = image
    for source_y in range(height):
        target_y = y + source_y
        if not 0 <= target_y < HEIGHT:
            continue
        for source_x in range(width):
            target_x = x + source_x
            if not 0 <= target_x < WIDTH:
                continue
            at = source_y * pitch + source_x * 4
            blue, green, red, source_alpha = pixels[at:at + 4]
            target = target_y * WIDTH + target_x
            if object_alpha == 255:
                framebuffer[target] = pack565(red, green, blue)
                continue
            alpha = (source_alpha * object_alpha + 127) // 255
            old_red, old_green, old_blue = unpack565(framebuffer[target])
            framebuffer[target] = pack565(blend(red, old_red, alpha), blend(green, old_green, alpha), blend(blue, old_blue, alpha))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=pathlib.Path)
    parser.add_argument("--json", type=pathlib.Path)
    args = parser.parse_args()
    package = (args.game_root / "DATA" / "common.pkg").read_bytes()
    records = list(entries(package, u32(package, 0)))
    primary = next((entry for entry in records if entry[0].upper() == "DATA/ASTEROID/00.GAI"), None)
    if not primary:
        raise ValueError("missing M20 primary")
    selected = [primary]
    for entry in sorted(records, key=lambda candidate: candidate[0]):
        if entry[0] == primary[0] or not (entry[0].startswith("DATA/") and entry[0].endswith(".gai")):
            continue
        try:
            decode_gai(payload(package, entry[1], entry[3], entry[4]))
            selected.append(entry)
        except (ValueError, struct.error):
            pass
        if len(selected) == 3:
            break
    if len(selected) != 3:
        raise ValueError("M20 selection does not have three decodable resources")
    images = [decode_gai(payload(package, entry[1], entry[3], entry[4])) for entry in selected]
    simple_entry = next((entry for entry in package_entries(package) if entry[0].casefold() == "data/planet/spu00.png"), None)
    if simple_entry is None:
        raise ValueError("configured M21 Simple resource is absent")
    simple_source = package_payload(package, simple_entry)
    simple_width, simple_height, simple_bytes, _ = decode_indexed_png(simple_source)
    simple_words = list(struct.unpack("<" + "H" * (simple_width * simple_height), simple_bytes))

    # First RunOneFrame: heartbeat frame 0, before M17/M20 have advanced.
    framebuffer = [CLEAR] * (WIDTH * HEIGHT)
    for x in range(WIDTH):
        framebuffer[x] = 0x07E0
    # DrawRuntimeHeartbeat overlays the horizontal marker with a full-height
    # vertical marker at x=0; reproducing only their intersection would leave
    # 719 RGB565 pixels different from the portable runtime.
    for y in range(HEIGHT):
        framebuffer[y * WIDTH] = 0xF800
    asteroid_x, asteroid_y = WIDTH // 2 - images[0][0] - 24, HEIGHT // 2 - images[0][1] // 2
    draw_bgra(framebuffer, images[0], asteroid_x, asteroid_y, 255)
    for y in range(simple_height):
        framebuffer[(96 + y) * WIDTH + 32:(96 + y) * WIDTH + 32 + simple_width] = simple_words[y * simple_width:(y + 1) * simple_width]
    draw_bgra(framebuffer, images[1], WIDTH // 2 + 24, HEIGHT // 2 - images[1][1] // 2, 255)
    draw_bgra(framebuffer, images[2], asteroid_x + 10, asteroid_y + 10, 128)
    raw = b"".join(struct.pack("<H", word) for word in framebuffer)
    result = {"status": "PRESENT", "frame": 0, "resources": [entry[0] for entry in selected],
              "simple_resource": "DATA/Planet/Spu00.png", "framebuffer_bytes": len(raw),
              "framebuffer_crc32": f"{__import__('zlib').crc32(raw) & 0xffffffff:08x}",
              "framebuffer_fnv64": f"{fnv64(raw):016x}"}
    text = json.dumps(result, indent=2, sort_keys=True)
    print(text)
    if args.json:
        args.json.write_text(text + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
