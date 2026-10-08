#!/usr/bin/env python3
"""Independent release-backed oracle for M22's fixed runtime UiTree frame."""
from __future__ import annotations
import argparse
import importlib.util
import pathlib
import struct
import sys
import zlib

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from probe_gai_release import entries, payload, u32

spec = importlib.util.spec_from_file_location('m21_release', HERE / 'probe_m21_release_scene.py')
m21 = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(m21)

FNV_OFFSET, FNV_PRIME = 0xCBF29CE484222325, 0x100000001B3
WIDTH, HEIGHT = 1280, 720


def fnv64(raw):
    value = FNV_OFFSET
    for byte in raw:
        value = ((value ^ byte) * FNV_PRIME) & 0xffffffffffffffff
    return value


def node(kind, name, parent, local, absolute, size, origin, depth, mode_w, active, disabled, children):
    encoded = name.encode()
    return (bytes((kind,)) + struct.pack('<I', len(encoded)) + encoded + struct.pack('<i', parent) +
            struct.pack('<8i', *local, *absolute, *size, *origin) +
            struct.pack('<Q', struct.unpack('<Q', struct.pack('<d', depth))[0]) +
            bytes((mode_w, active, disabled)) + struct.pack('<I', children))


def select(package):
    records = list(entries(package, u32(package, 0)))
    primary = next(entry for entry in records if entry[0].upper() == 'DATA/ASTEROID/00.GAI')
    selected = [primary]
    for entry in sorted(records, key=lambda candidate: candidate[0]):
        if entry[0] == primary[0] or not (entry[0].startswith('DATA/') and entry[0].endswith('.gai')):
            continue
        try:
            m21.decode_gai(payload(package, entry[1], entry[3], entry[4]))
            selected.append(entry)
        except (ValueError, struct.error):
            pass
        if len(selected) == 3:
            return selected
    raise ValueError('M20 selection does not have three decodable resources')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('game_root', type=pathlib.Path)
    args = parser.parse_args()
    package = (args.game_root / 'DATA' / 'common.pkg').read_bytes()
    selected = select(package)
    images = [m21.decode_gai(payload(package, item[1], item[3], item[4])) for item in selected]
    simple_entry = next(item for item in m21.package_entries(package) if item[0].casefold() == 'data/planet/spu00.png')
    simple_width, simple_height, simple_bytes, _ = m21.decode_indexed_png(m21.package_payload(package, simple_entry))
    asteroid_x, asteroid_y = WIDTH // 2 - images[0][0] - 24, HEIGHT // 2 - images[0][1] // 2
    secondary_x, secondary_y = WIDTH // 2 + 24, HEIGHT // 2 - images[1][1] // 2
    raw = node(1, 'm22-root', -1, (0, 0), (0, 0), (WIDTH, HEIGHT), (0, 0), 0.0, 0, 1, 0, 1)
    raw += node(1, 'm22-content', 0, (0, 0), (0, 0), (WIDTH, HEIGHT), (0, 0), 0.0, 0, 1, 0, 5)
    raw += node(3, 'm20-asteroid', 1, (asteroid_x, asteroid_y), (asteroid_x, asteroid_y), images[0][:2], (0, 0), 0.0, 0, 1, 0, 0)
    raw += node(1, 'm22-simple-panel', 1, (0, 0), (0, 0), (WIDTH, HEIGHT), (0, 0), -5.0, 0, 1, 0, 1)
    raw += node(1, 'm22-scroll-panel', 3, (0, 0), (0, 0), (WIDTH, HEIGHT), (0, 0), 0.0, 0, 1, 0, 2)
    raw += node(2, 'm21-simple-spu00', 4, (32, 96), (32, 96), (128, 60), (0, 0), 0.0, 0, 1, 0, 0)
    raw += node(2, 'm22-modew-hidden', 4, (1, 1), (1, 1), (1, 1), (0, 0), -1.0, 1, 1, 0, 0)
    raw += node(3, 'm20-secondary', 1, (secondary_x, secondary_y), (secondary_x, secondary_y), images[1][:2], (0, 0), -10.0, 0, 1, 0, 0)
    raw += node(3, 'm20-overlay', 1, (asteroid_x + 10, asteroid_y + 10), (asteroid_x + 10, asteroid_y + 10), images[2][:2], (0, 0), -20.0, 0, 1, 0, 0)
    raw += node(2, 'm22-inactive', 1, (0, 0), (0, 0), (1, 1), (0, 0), -100.0, 0, 0, 0, 0)
    # Independent framebuffer construction follows the existing M21 release
    # oracle but this script owns the expected M22 tree serialization.
    simple_words = list(struct.unpack('<' + 'H' * (simple_width * simple_height), simple_bytes))
    framebuffer = [m21.CLEAR] * (WIDTH * HEIGHT)
    for x in range(WIDTH): framebuffer[x] = 0x07e0
    for y in range(HEIGHT): framebuffer[y * WIDTH] = 0xf800
    m21.draw_bgra(framebuffer, images[0], asteroid_x, asteroid_y, 255)
    for y in range(simple_height): framebuffer[(96 + y) * WIDTH + 32:(96 + y) * WIDTH + 32 + simple_width] = simple_words[y * simple_width:(y + 1) * simple_width]
    m21.draw_bgra(framebuffer, images[1], secondary_x, secondary_y, 255)
    m21.draw_bgra(framebuffer, images[2], asteroid_x + 10, asteroid_y + 10, 128)
    pixels = b''.join(struct.pack('<H', word) for word in framebuffer)
    print('M22 RUNTIME ORACLE PASS '
          f'tree_crc32={zlib.crc32(raw) & 0xffffffff:08x} tree_fnv64={fnv64(raw):016x} tree_bytes={len(raw)} '
          f'frame_crc32={zlib.crc32(pixels) & 0xffffffff:08x} frame_fnv64={fnv64(pixels):016x} frame_bytes={len(pixels)} '
          f'resources={"|".join(item[0] for item in selected)}')


if __name__ == '__main__':
    main()
