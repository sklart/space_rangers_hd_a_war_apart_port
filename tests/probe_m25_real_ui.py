#!/usr/bin/env python3
"""Independent release loading-bar geometry and RGB565 oracle.

The selected config subtree is read from Main.dat; no decoded game asset is
written to the repository. Rendering is local to the subtree's 321x37 bounds.
"""
from __future__ import annotations

import argparse
import json
import mmap
from pathlib import Path
import struct

import probe_gai_release as gi_oracle
import probe_m23_text_release as config
from probe_ui_images_release import decode_dat, entries, payload

PATH = "ML#0/AB#0/Panel#0/Panel#0/Panel#1/Panel#0/Panel#0"
WIDTH, HEIGHT = 321, 37


def fingerprint(data: bytes) -> dict:
    return gi_oracle.fp(data)


def i32(value: int) -> bytes:
    return struct.pack("<i", value)


def node_bytes(kind: int, name: str, parent: int, pos: tuple[int, int],
               size: tuple[int, int], children: int) -> bytes:
    encoded = name.encode("ascii")
    return (bytes([kind]) + struct.pack("<I", len(encoded)) + encoded + i32(parent)
            + b"".join(i32(value) for value in (*pos, *pos, *size, 0, 0))
            + struct.pack("<d", 0.0) + bytes((0, 1, 0)) + struct.pack("<I", children))


def render(root: Path) -> dict:
    parsed = config.parse_blocks(decode_dat(root / "CFG/Main.dat"))
    panel = next(n for n in config.nodes(parsed) if n.unique_path == PATH)
    assert panel.name == "Panel" and panel.last("Name") == "PLBar"
    assert panel.last("Size") == f"{WIDTH},{HEIGHT}"
    children = [n for n in panel.children if n.name == "Image"]
    assert len(children) == 17 and not any(n.children for n in children)
    cache = {path.replace("/", ".").casefold(): name.replace("\\", "/").casefold()
             for path, name in config.data_entries(decode_dat(root / "CFG/CacheData.dat", 0xEA8F3F37))}
    resources = {}
    with (root / "DATA/forms.pkg").open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
        index = {entry[0].replace("\\", "/").casefold(): entry for entry in entries(mapped)}
        for child in children:
            raw = child.last("Image")
            assert raw.startswith("GI,")
            key = raw[3:]
            path = cache[key.casefold()]
            entry = index[path]
            source = payload(mapped, entry)
            metadata, pixels = gi_oracle.decode2(source)
            assert metadata["format"] == 2
            resources[key] = {"path": entry[0], "source": fingerprint(source),
                              "metadata": metadata, "pixels": pixels}
    framebuffer = [0] * (WIDTH * HEIGHT)
    # UiObject::Attach inserts equal-depth siblings before existing siblings.
    for child in reversed(children):
        key = child.last("Image")[3:]
        resource = resources[key]
        image = resource["metadata"]["decoded"]
        x, y = (int(v) for v in child.last("Pos").split(","))
        cw, ch = (int(v) for v in child.last("Size").split(","))
        assert (cw, ch) == (image["width"], image["height"])
        pixels = resource["pixels"]
        for sy in range(ch):
            for sx in range(cw):
                dx, dy = x + sx, y + sy
                if not 0 <= dx < WIDTH or not 0 <= dy < HEIGHT:
                    continue
                at = (sy * cw + sx) * 4
                blue, green, red, alpha = pixels[at:at + 4]
                if alpha == 0:
                    continue
                target = dy * WIDTH + dx
                old = framebuffer[target]
                dr, dg, db = ((old >> 11 & 31) * 255 // 31,
                              (old >> 5 & 63) * 255 // 63,
                              (old & 31) * 255 // 31)
                blend = lambda src, dst: (src * alpha + dst * (255 - alpha) + 127) // 255
                r, g, b = blend(red, dr), blend(green, dg), blend(blue, db)
                framebuffer[target] = (r >> 3 << 11) | (g >> 2 << 5) | (b >> 3)
    frame_bytes = b"".join(struct.pack("<H", pixel) for pixel in framebuffer)
    tree = bytearray(node_bytes(1, "PLBar", -1, (0, 0), (WIDTH, HEIGHT), len(children)))
    for child in reversed(children):
        x, y = (int(v) for v in child.last("Pos").split(","))
        w, h = (int(v) for v in child.last("Size").split(","))
        tree += node_bytes(2, child.last("Name"), 0, (x, y), (w, h), 0)
    return {"path": PATH, "nodes": 18, "max_depth": 2, "visual_leaves": 17,
            "control_distribution": {"Panel": 1, "Image": 17},
            "resources": {key: {"path": value["path"], "source": value["source"],
                                "bounds": value["metadata"]["bounds"]}
                          for key, value in sorted(resources.items())},
            "tree": fingerprint(bytes(tree)), "frame": fingerprint(frame_bytes)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    args = parser.parse_args()
    print(json.dumps(render(args.game_root), ensure_ascii=False, indent=2))
