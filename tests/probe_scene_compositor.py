"""Independent Python oracle for the M19 real-resource scene."""
from __future__ import annotations

import argparse
import json
import pathlib
import struct
import zlib

from probe_gai_release import decode2, entries, fp, frame, gai, payload, u32

FNV_OFFSET, FNV_PRIME = 0xCBF29CE484222325, 0x100000001B3
WIDTH, HEIGHT, CLEAR = 1280, 720, 0x0010


def pack565(red, green, blue):
    return ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)


def unpack565(value):
    return ((value >> 11) & 31) << 3, ((value >> 5) & 63) << 2, (value & 31) << 3


def blend(source, destination, alpha):
    return (source * alpha + destination * (255 - alpha) + 127) // 255


def draw(framebuffer, image, x, y, alpha):
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
            if alpha == 255:
                framebuffer[target] = pack565(red, green, blue)
                continue
            effective_alpha = (source_alpha * alpha + 127) // 255
            old_red, old_green, old_blue = unpack565(framebuffer[target])
            framebuffer[target] = pack565(
                blend(red, old_red, effective_alpha),
                blend(green, old_green, effective_alpha),
                blend(blue, old_blue, effective_alpha),
            )


def decode_resource(package, entry):
    source = payload(package, entry[1], entry[3], entry[4])
    container = gai(source)
    _, gi, _, _ = frame(source, container, 0)
    if not gi:
        raise ValueError("empty frame 0")
    metadata, pixels = decode2(gi)
    decoded = metadata["decoded"]
    return decoded["width"], decoded["height"], decoded["pitch"], pixels


def append_u32(stream, value):
    stream.extend(struct.pack("<I", value & 0xffffffff))


def fingerprint(sprites):
    canonical = bytearray()
    for item in sorted(enumerate(sprites), key=lambda pair: pair[1]["layer"]):
        _, sprite = item
        name = sprite["id"].encode("utf-8")
        append_u32(canonical, len(name)); canonical.extend(name)
        for field in (sprite["layer"], sprite["x"], sprite["y"]): append_u32(canonical, field)
        canonical.append(sprite["alpha"])
        width, height, pitch, pixels = sprite["image"]
        for field in (width, height, pitch): append_u32(canonical, field)
        canonical.extend(pixels)
    value = FNV_OFFSET
    for byte in canonical: value = ((value ^ byte) * FNV_PRIME) & 0xffffffffffffffff
    return {"canonical_bytes": len(canonical), "crc32": f"{zlib.crc32(canonical):08x}", "fnv64": f"{value:016x}"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=pathlib.Path)
    args = parser.parse_args()
    package = (args.game_root / "DATA" / "common.pkg").read_bytes()
    all_entries = list(entries(package, u32(package, 0)))
    primary = next((item for item in all_entries if item[0].upper() == "DATA/ASTEROID/00.GAI"), None)
    if not primary: raise ValueError("missing DATA/Asteroid/00.gai")
    asteroid = decode_resource(package, primary)
    selected, secondary = None, None
    for item in sorted(all_entries, key=lambda candidate: candidate[0]):
        if item[0] == primary[0] or not (item[0].startswith("DATA/") and item[0].endswith(".gai")):
            continue
        try:
            secondary = decode_resource(package, item); selected = item; break
        except (ValueError, zlib.error, struct.error):
            pass
    if not selected: raise ValueError("no second decodable DATA/*.gai resource")
    asteroid_x, asteroid_y = WIDTH // 2 - asteroid[0] - 24, HEIGHT // 2 - asteroid[1] // 2
    secondary_x, secondary_y = WIDTH // 2 + 24, HEIGHT // 2 - secondary[1] // 2
    sprites = [
        {"id": "asteroid", "resource": primary[0], "frame": 0, "image": asteroid, "x": asteroid_x, "y": asteroid_y, "layer": 0, "alpha": 255},
        {"id": "secondary", "resource": selected[0], "frame": 0, "image": secondary, "x": secondary_x, "y": secondary_y, "layer": 10, "alpha": 255},
        {"id": "asteroid-overlay", "resource": primary[0], "frame": 0, "image": asteroid, "x": asteroid_x + 10, "y": asteroid_y + 10, "layer": 20, "alpha": 128},
    ]
    framebuffer = [CLEAR] * (WIDTH * HEIGHT)
    for sprite in sorted(enumerate(sprites), key=lambda pair: pair[1]["layer"]):
        item = sprite[1]
        draw(framebuffer, item["image"], item["x"], item["y"], item["alpha"])
    raw = b"".join(struct.pack("<H", pixel) for pixel in framebuffer)
    result = {
        "framebuffer": fp(raw), "scene": fingerprint(sprites),
        "sprites": [{key: value for key, value in sprite.items() if key != "image"} for sprite in sprites],
    }
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
