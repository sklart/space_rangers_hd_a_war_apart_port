#!/usr/bin/env python3
"""Independent, asset-free M24 controls/layout/frame oracle.

The scene uses one-pixel RGB565 images and the M23 tiny AFT glyph A.  All
positions, draw order and hashes are calculated here, never read from C++.
"""
from __future__ import annotations

import struct

from probe_m23_ui_text import Node, fingerprint, tree_bytes


SLOTS = ("top-left", "top-right", "bottom-left", "bottom-right",
         "left", "right", "top", "bottom", "texture")
WINDOW_SIZE = (10, 8)
WINDOW_POS = (1, 1)
ZONE_POINTS = ((4, 5), (5, 6), (6, 6), (4, 6), (3, 5), (7, 7))


def layout() -> list[tuple[str, tuple[int, int], tuple[int, int], int, int]]:
    width, height = WINDOW_SIZE
    # x/y modes: LeftFill=0, Center=4, TopFill=0.
    return [
        ("top-left", (0, 0), (1, 1), 4, 4),
        ("top-right", (width - 1, 0), (1, 1), 4, 4),
        ("bottom-left", (0, height - 1), (1, 1), 4, 4),
        ("bottom-right", (width - 1, height - 1), (1, 1), 4, 4),
        ("left", (0, 1), (1, height - 2), 4, 0),
        ("right", (width - 1, 1), (1, height - 2), 4, 0),
        ("top", (1, 0), (width - 2, 1), 0, 4),
        ("bottom", (1, height - 1), (width - 2, 1), 0, 4),
        ("texture", (1, 1), (width - 2, height - 2), 0, 0),
    ]


def window_layout_bytes() -> bytes:
    result = bytearray(struct.pack("<iiiiii", 10, 8, 1, 1, 9, 7))
    for _, pos, size, x_mode, y_mode in layout():
        result.extend(struct.pack("<iiiiBBB", *pos, *size, 1, x_mode, y_mode))
    return bytes(result)


def zone_bytes(circle: bool) -> bytes:
    result = bytearray(struct.pack("<Bi", int(circle), len(ZONE_POINTS)))
    for x, y in ZONE_POINTS:
        rect = 4 <= x < 6 and 5 <= y < 7
        circle_hit = (x - 5) ** 2 + (y - 6) ** 2 <= 1
        result.extend(struct.pack("<iiB", x, y, circle_hit if circle else rect))
    return bytes(result)


def graph_state_bytes(state: str) -> bytes:
    hovered, down, visible, color, caption_x = {
        "A": (0, 0, 0, 0xFFFF, 0),
        "B": (1, 0, 1, 0x07E0, 0),
        "C": (0, 1, 2, 0xFFE0, 1),
    }[state]
    result = bytearray(struct.pack("<BBBBBBBBiiHiH", 0, 0, hovered, down,
                                   0, 0, visible, 1, caption_x, 0,
                                   color, 0, 0))
    for slot in range(7):
        present = slot in (0, 1, 2)
        result.extend(struct.pack("<BiiB", present, 0, 0, present and slot == visible))
    return bytes(result)


def scene_tree(state: str) -> Node:
    root = Node("root", 1, (0, 0), (0, 0), (12, 10))
    window = Node("window", 6, WINDOW_POS, WINDOW_POS, WINDOW_SIZE)
    root.children.append(window)
    by_name = {name: (pos, size) for name, pos, size, _, _ in layout()}
    # Equal-depth insertion puts the most recently created border first.
    for name in reversed(("left", "right", "top", "bottom", "top-left",
                          "top-right", "bottom-left", "bottom-right", "texture")):
        pos, size = by_name[name]
        absolute = (WINDOW_POS[0] + pos[0], WINDOW_POS[1] + pos[1])
        window.children.append(Node("window-" + name, 2, pos, absolute,
                                    size, depth=1e6))
    a = Node("button-a", 5, (2, 1), (3, 2), (5, 6), depth=20)
    window.children.append(a)
    for slot, name in ((2, "down"), (1, "normal-a"), (0, "normal")):
        a.children.append(Node("a-" + name, 2, (0, 0), (3, 2), (1, 1),
                               depth=1, active=slot == {"A": 0, "B": 1, "C": 2}[state]))
    caption_local = (1, 0) if state == "C" else (0, 0)
    a.children.append(Node("caption", 4, caption_local,
                           (3 + caption_local[0], 2), (5, 6), depth=-9999))
    b = Node("button-b", 5, (7, 2), (8, 3), (2, 2), depth=10)
    b.children.append(Node("b-normal", 2, (0, 0), (8, 3), (1, 1), depth=1))
    window.children.append(b)
    window.children.append(Node("label", 4, (0, 0), (1, 1), (5, 5), depth=5))
    window.children.append(Node("gi", 3, (7, 5), (8, 6), (2, 2), depth=4))
    window.children.append(Node("zone", 7, (3, 4), (4, 5), (2, 2)))
    return root


def frame(state: str) -> bytes:
    pixels = [0] * 120

    def rect(x: int, y: int, width: int, height: int, color: int) -> None:
        for py in range(max(0, y, 1), min(y + height, 9, 10)):
            for px in range(max(0, x, 1), min(x + width, 11, 12)):
                pixels[py * 12 + px] = color

    rect(1, 1, 10, 8, 0x001F)  # tiled Window border and texture
    rect(3, 2, 1, 1, {"A": 0xF800, "B": 0x07E0, "C": 0xFFE0}[state])
    # M23 fixture glyph A: opaque 2x2 at pen X+2 and baseline Y+3.
    rect(5 + (state == "C"), 5, 2, 2,
         {"A": 0xFFFF, "B": 0x07E0, "C": 0xFFE0}[state])
    rect(8, 3, 1, 1, 0xF800)  # GraphButton B
    rect(3, 4, 2, 2, 0xFFFF)  # nested M23 Label
    rect(8, 6, 2, 2, 0xF800 if state == "A" else 0x07E0)  # M20 GI
    return struct.pack("<120H", *pixels)


def expected() -> dict[str, tuple[str, str]]:
    values = {"window_layout": fingerprint(window_layout_bytes()),
              "zone_rect": fingerprint(zone_bytes(False)),
              "zone_circle": fingerprint(zone_bytes(True))}
    for state in "ABC":
        values["tree_" + state] = fingerprint(tree_bytes(scene_tree(state)))
        values["graph_" + state] = fingerprint(graph_state_bytes(state))
        values["frame_" + state] = fingerprint(frame(state))
    return values


if __name__ == "__main__":
    for label, pair in expected().items():
        print(label, *pair)
