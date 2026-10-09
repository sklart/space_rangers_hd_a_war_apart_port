#!/usr/bin/env python3
"""Independent release ScrollBar arithmetic from decoded Main.dat inventory."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct

from probe_gai_release import fp


def oracle(scroll: dict, panels: dict) -> dict:
    bar = scroll["deterministic_candidate"]
    assert bar["path"] == "ML#0/Film#0/Panel#0/Panel#0/Panel#0/ScrollBar#0"
    assert bar["orientation"] == "x" and bar["size"] == "251,19"
    number = bar["numeric"]
    assert number["KindCalc"] == 0 and number["Min"] <= number["Position"] <= number["Max"]
    axis = lambda slot: bar["images"][slot]["dimensions"][0]
    up, down = axis("ImageUpN"), axis("ImageDownN")
    top, bottom = axis("ImageTopN"), axis("ImageBottomN")
    length = int(bar["size"].split(",")[0])
    track = length - up - down
    full_range = number["Max"] - number["Min"] + 1
    raw_thumb = number["PageSize"] * track // full_range
    min_thumb = top + bottom if top + bottom >= raw_thumb else 0
    thumb = min_thumb or raw_thumb
    before = ((number["Position"] - number["Min"]) * (track - min_thumb) //
              (number["Max"] - number["Min"])) if min_thumb else (
                  (number["Position"] - number["Min"]) * track // full_range)
    after = track - before - thumb
    # Upstream's Fill image spans include the adjacent thumb cap.
    placements = {
        "up": (0, up), "before": (up, top + before),
        "top": (up + before, top),
        "center": (up + top + before, thumb - top - bottom),
        "bottom": (up + before + thumb - bottom, bottom),
        "after": (before + thumb - bottom + down, bottom + after),
        "down": (length - down, down),
    }
    panel = panels["deterministic_candidate"]
    assert panel["path"] == "ML#0/Achievements#0/Panel#0/Panel#0/PanelScrollBar#0"
    rectangle = tuple(map(int, panel["rect_y"].split(",")))
    assert rectangle == (903, 116, 923, 629)
    panel_size = tuple(map(int, panel["size"].split(",")))
    assert panel_size == (831, 540)
    layout = (track, thumb, before, after, up, down, top, bottom,
              *(value for pair in placements.values() for value in pair))
    panel_layout = (*rectangle, 20, 513, 0, 539, 540, 0)
    return {"scroll_path": bar["path"], "panel_path": panel["path"],
            "scroll_layout": {"track": track, "thumb": thumb,
                              "before": before, "after": after,
                              "minimum_thumb": min_thumb, "placements": placements,
                              "crc_fnv": fp(struct.pack("<" + "i" * len(layout), *layout))},
            "panel_layout": {"rect_y": rectangle, "bar_size": (20, 513),
                             "range": (0, 539), "page": 540, "position": 0,
                             "crc_fnv": fp(struct.pack("<10i", *panel_layout))}}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("scroll_inventory", type=Path)
    parser.add_argument("panel_inventory", type=Path)
    args = parser.parse_args()
    load = lambda path: json.loads(path.read_text(encoding="utf-8-sig"))
    print(json.dumps(oracle(load(args.scroll_inventory), load(args.panel_inventory)), indent=2))
