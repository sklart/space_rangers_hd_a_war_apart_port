#!/usr/bin/env python3
"""Independent asset-free AFT, layout, UI tree and RGB565 frame oracle for M23."""
from __future__ import annotations

from dataclasses import dataclass, field
import pathlib
import struct
import sys
import zlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import probe_aft_release


def fingerprint(data: bytes) -> tuple[str, str]:
    fnv = 0xCBF29CE484222325
    for byte in data:
        fnv = ((fnv ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return f"{zlib.crc32(data):08x}", f"{fnv:016x}"


def rle(alpha: bool) -> bytes:
    rows = bytearray()
    for row in range(2):
        rows.append(0x82)
        if alpha:
            rows.extend((192 if row else 64, 192 if row else 144))
        rows.append(0)
    return struct.pack("<IIII", len(rows), 2, 2, 0) + rows


def aft_fixture() -> bytes:
    codes = (ord(" "), ord("A"), ord("B"), ord("C"), ord("Ё"), ord("я"))
    source = bytearray(32 + 64 * len(codes))
    source[0:3] = b"aft"
    struct.pack_into("<I", source, 4, 1)
    struct.pack_into("<I", source, 8, len(codes))
    struct.pack_into("<I", source, 12, 3)
    struct.pack_into("<I", source, 20, 5)
    for index, code in enumerate(codes):
        at = 32 + 64 * index
        struct.pack_into("<Iiii", source, at, code, -1 if index == 5 else 1,
                         2 if index == 0 else 3, 2 if index == 4 else 1)
        if index == 0:
            continue
        for pass_index in range(2):
            is_alpha = pass_index == 1
            if (index == 1 and is_alpha) or (index == 2 and not is_alpha):
                continue
            plane = at + 16 + 24 * pass_index
            encoded = rle(is_alpha)
            struct.pack_into("<iiiiII", source, plane, -1 if index == 5 else 0,
                             -2 if index == 4 else 0, 2, 2, len(source), len(encoded))
            source.extend(encoded)
    return bytes(source)


@dataclass
class Node:
    name: str
    kind: int
    local: tuple[int, int]
    absolute: tuple[int, int]
    size: tuple[int, int]
    origin: tuple[int, int] = (0, 0)
    depth: float = 0.0
    mode_w: bool = False
    active: bool = True
    children: list[Node] = field(default_factory=list)


def tree(scroll: int) -> Node:
    root = Node("root", 1, (0, 0), (0, 0), (12, 10))
    root.children.append(Node("background", 2, (0, 0), (0, 0), (12, 10), depth=100))
    panel = Node("panel", 1, (1, 1), (1, 1), (10, 8), depth=10)
    root.children.append(panel)
    panel.children.append(Node("image", 2, (0, 0), (1, 1), (10, 8), depth=100))
    panel.children.append(Node("scroll-label", 4, (0, 0), (1 - scroll, 1),
                               (8, 7), depth=5, mode_w=True))
    nested = Node("nested", 1, (3, 2), (4, 3), (6, 5), origin=(1, 0))
    panel.children.append(nested)
    nested.children.append(Node("gi", 3, (3, 3), (7, 6), (2, 2), depth=5))
    nested.children.append(Node("nested-label", 4, (0, 0), (4, 3), (6, 5)))
    panel.children.append(Node("inactive", 4, (0, 0), (1, 1), (10, 8),
                               depth=-10, active=False))
    return root


def tree_bytes(root: Node) -> bytes:
    result = bytearray()
    counter = 0

    def walk(node: Node, parent: int) -> None:
        nonlocal counter
        own = counter
        counter += 1
        name = node.name.encode("utf-8")
        result.extend(struct.pack("<BI", node.kind, len(name)))
        result.extend(name)
        result.extend(struct.pack("<i", parent))
        result.extend(struct.pack("<iiiiiiii", *node.local, *node.absolute,
                                  *node.size, *node.origin))
        result.extend(struct.pack("<dBBBI", node.depth, node.mode_w, node.active,
                                  False, len(node.children)))
        for child in node.children:
            walk(child, own)

    walk(root, -1)
    return bytes(result)


def layout_bytes(scroll: int, text: str) -> bytes:
    result = bytearray()
    for name, content, position in (("scroll-label", text, (1 - scroll, 1)),
                                    ("nested-label", "A", (4, 3))):
        encoded = content.encode("utf-16le")
        ascii_name = name.encode("ascii")
        result.extend(struct.pack("<I", len(ascii_name)))
        result.extend(ascii_name)
        result.extend(struct.pack("<I", len(encoded)))
        result.extend(encoded)
        result.extend(struct.pack("<iiiiiiiiI", *position, 0, 0,
                                  5 * len(content), 2, 5 * len(content), 4, 1))
    return bytes(result)


def frame(scroll: int, text: str, gi_color: int) -> bytes:
    pixels = [0] * (12 * 10)

    def rect(x: int, y: int, width: int, height: int, color: int,
             clip: tuple[int, int, int, int]) -> None:
        left, top, right, bottom = clip
        for py in range(max(y, top, 0), min(y + height, bottom, 10)):
            for px in range(max(x, left, 0), min(x + width, right, 12)):
                pixels[py * 12 + px] = color

    rect(0, 0, 12, 10, 0x001F, (0, 0, 12, 10))
    rect(1, 1, 10, 8, 0xF800, (1, 1, 11, 9))
    # Label Top/Left gives baseline = absolute Y + AboveBaseline (3).
    # Fixture A is an opaque 2x2 plane at the pen position; advance is 5.
    for index, _ in enumerate(text):
        rect(3 - scroll + index * 5, 4, 2, 2, 0xFFFF, (1, 1, 9 - scroll, 8))
    # Nested panel has origin X=1; its half-open clip is [3,9) x [3,8).
    rect(7, 6, 2, 2, gi_color, (3, 3, 9, 8))
    rect(6, 6, 2, 2, 0x07E0, (4, 3, 10, 8))
    return struct.pack("<" + "H" * len(pixels), *pixels)


def main() -> None:
    source = aft_fixture()
    font = probe_aft_release.aft(source)
    if font["glyph_count"] != 6 or font["above_baseline"] != 3:
        raise AssertionError("synthetic AFT metrics")
    print("font_source", *fingerprint(source))
    print("font_structural", font["structural"])
    for name, scroll, text, gi_color in (("A", 0, "A", 0xF800),
                                         ("B", 1, "AA", 0x07E0)):
        print("layout_" + name, *fingerprint(layout_bytes(scroll, text)))
        print("tree_" + name, *fingerprint(tree_bytes(tree(scroll))))
        print("frame_" + name, *fingerprint(frame(scroll, text, gi_color)))


if __name__ == "__main__":
    main()
