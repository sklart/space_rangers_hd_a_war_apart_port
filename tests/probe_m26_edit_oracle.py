#!/usr/bin/env python3
"""Independent release Edit caret oracle using Main.dat and raw AFT metrics."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct

import probe_aft_release as aft
import probe_m23_real_label as font_probe
import probe_m23_text_release as config
from probe_ui_images_release import decode_dat


PATH = "ML#0/Info#0/Panel#0/Panel#4/Panel#11/Edit#0"


def oracle(root: Path) -> dict:
    nodes = {node.unique_path: node for node in
             config.nodes(config.parse_blocks(decode_dat(root / "CFG/Main.dat")))}
    edit = nodes[PATH]
    parents = [nodes["ML#0/Info#0/Panel#0/Panel#4"],
               nodes["ML#0/Info#0/Panel#0/Panel#4/Panel#11"]]
    x, y = (sum(int(node.last("Pos").split(",")[axis]) for node in (*parents, edit))
            for axis in (0, 1))
    width, height = map(int, edit.last("Size").split(","))
    assert (x, y, width, height) == (589, 344, 99, 17)
    assert edit.last("Font") == "Font.2Normal"
    assert edit.last("TextColor") == "0,0,0"
    assert (edit.last("CursorColor") or "255,0,0") == "255,0,0"
    package, resource, source = font_probe.resolve_font(root, edit.last("Font"))
    metrics = aft.aft(source)
    glyphs = font_probe.glyphs(source)
    text = "123"
    caret_x = x + 2 + sum(glyphs[char][0] for char in text)
    baseline = (y + y + height) // 2 - (
        metrics["above_baseline"] + metrics["below_baseline"]) // 2 + metrics["above_baseline"]
    caret_y = baseline - (metrics["above_baseline"] - 1)
    caret_height = metrics["above_baseline"] + metrics["below_baseline"]
    frame = bytearray(1280 * 720 * 2)
    for px in (caret_x, caret_x + 1):
        for py in range(caret_y, caret_y + caret_height):
            if x <= px < x + width and y <= py < y + height:
                struct.pack_into("<H", frame, (py * 1280 + px) * 2, 0xf800)
    zero_frame = aft.fingerprint(bytes(len(frame)))
    return {"path": PATH, "font_package": package, "font_resource": resource,
            "font_source": aft.fingerprint(source), "text": text,
            "text_color_rgb565": 0, "caret_color_rgb565": 0xf800,
            "bounds": (x, y, width, height), "caret": (caret_x, caret_y, 2, caret_height),
            "unfocused_frame": zero_frame,
            "focused_caret_off_frame": zero_frame,
            "focused_caret_on_frame": aft.fingerprint(bytes(frame)),
            "frame": aft.fingerprint(bytes(frame))}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    args = parser.parse_args()
    print(json.dumps(oracle(args.game_root), indent=2))
