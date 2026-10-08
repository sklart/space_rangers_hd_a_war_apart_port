#!/usr/bin/env python3
"""Independent M22 mixed UI-tree and framebuffer oracle.

This deliberately describes the fixture rather than consuming the C++ test,
its serialized tree, or its framebuffer.  Rectangles are half-open.
"""
import struct
import zlib

OFFSET = 0xCBF29CE484222325
PRIME = 0x100000001B3
BLUE, GREEN, RED, HALF_RED_OVER_BLUE = 0x001F, 0x07E0, 0xF800, 0x780F


def fnv64(data):
    value = OFFSET
    for byte in data:
        value = ((value ^ byte) * PRIME) & 0xFFFFFFFFFFFFFFFF
    return value


def fingerprint_words(words):
    raw = struct.pack('<%dH' % len(words), *words)
    return zlib.crc32(raw) & 0xFFFFFFFF, fnv64(raw), len(raw)


def node(kind, name, parent, local, absolute, size, origin, depth, mode_w, active, disabled, children):
    encoded = name.encode('utf-8')
    return (bytes((kind,)) + struct.pack('<I', len(encoded)) + encoded + struct.pack('<i', parent) +
            struct.pack('<8i', *local, *absolute, *size, *origin) +
            struct.pack('<Q', struct.unpack('<Q', struct.pack('<d', depth))[0]) +
            bytes((mode_w, active, disabled)) + struct.pack('<I', children))


def tree(frame_b):
    # The root child traversal is background, panel-a, inactive.  panel-b's
    # equal-depth Trans/GI order follows M22's newer-before-existing insertion.
    panel_b_children = (('gi', 3, (0, 1), (2, 2), (2, 2), (0, 0), 6.0),
                        ('trans', 2, (-1, 0), (1, 1), (2, 1), (0, 0), 5.0),
                        ('alpha', 2, (0, 0), (1, 1) if frame_b else (2, 1), (1, 1), (1, 0), 0.0)) if frame_b else (
                        ('trans', 2, (-1, 0), (1, 1), (2, 1), (0, 0), 5.0),
                        ('gi', 3, (0, 1), (2, 2), (2, 2), (0, 0), 5.0),
                        ('alpha', 2, (0, 0), (2, 1), (1, 1), (1, 0), 0.0))
    raw = node(1, 'root', -1, (0, 0), (0, 0), (6, 4), (0, 0), 0.0, 0, 1, 0, 3)
    raw += node(2, 'background', 0, (0, 0), (0, 0), (6, 4), (0, 0), 100.0, 0, 1, 0, 0)
    raw += node(1, 'panel-a', 0, (1, 0), (1, 0), (4, 4), (1, 0), 10.0, 0, 1, 0, 1)
    raw += node(1, 'panel-b', 2, (1, 1), (2, 1), (3, 2), (1, 0), 0.0, 0, 1, 0, 3)
    for name, kind, local, absolute, size, origin, depth in panel_b_children:
        raw += node(kind, name, 3, local, absolute, size, origin, depth, int(name == 'alpha'), 1, 0, 0)
    raw += node(2, 'inactive', 0, (0, 0), (0, 0), (6, 4), (0, 0), -100.0, 0, 0, 0, 0)
    return zlib.crc32(raw) & 0xFFFFFFFF, fnv64(raw), len(raw)


def framebuffer(frame_b):
    pixels = [BLUE] * 24
    # panel-a has origin x=1, panel-b has origin x=1; therefore the nested
    # clip is x=[1,4), y=[1,3).  The Simple background precedes them.
    pixels[1 * 6 + 2] = GREEN  # keyed first Trans pixel is transparent.
    color = GREEN if frame_b else RED
    pixels[2 * 6 + 2] = color
    pixels[2 * 6 + 3] = color
    if not frame_b:
        pixels[1 * 6 + 1] = HALF_RED_OVER_BLUE
    # Frame B scrolls the ModeW alpha leaf left beyond panel-b's clip.
    return fingerprint_words(pixels)


def main():
    tree_a, frame_a = tree(False), framebuffer(False)
    tree_b, frame_b = tree(True), framebuffer(True)
    assert tree_a == (0xAB1BFF76, 0x6F04E776692C26F3, 496), tree_a
    assert frame_a == (0x800CAFB4, 0xE1E077394C109D07, 48), frame_a
    assert tree_b == (0xE21F4881, 0xDA9CF654E3BF116A, 496), tree_b
    assert frame_b == (0x1D127015, 0xE2076F2B18E93B07, 48), frame_b
    print('M22 UI TREE ORACLE PASS '
          f'tree_a_crc32={tree_a[0]:08x} tree_a_fnv64={tree_a[1]:016x} '
          f'frame_a_crc32={frame_a[0]:08x} frame_a_fnv64={frame_a[1]:016x} '
          f'tree_b_crc32={tree_b[0]:08x} tree_b_fnv64={tree_b[1]:016x} '
          f'frame_b_crc32={frame_b[0]:08x} frame_b_fnv64={frame_b[1]:016x}')


if __name__ == '__main__':
    main()
