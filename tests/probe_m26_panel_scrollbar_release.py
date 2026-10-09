#!/usr/bin/env python3
"""Read-only physical PanelScrollBar inventory with referenced ScrollBar styles."""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path

import probe_m23_text_release as m23
import probe_m24_controls_release as m24
from probe_m26_scrollbar_release import IMAGE_NAMES
from probe_ui_images_release import decode_dat


BASE = frozenset(("Style", "Pos", "PosZ", "Size", "Sme", "Name", "Active",
                  "Help", "MVUpdate", "MouseBlocking", "MouseBlockingTest"))
PANEL = frozenset(("StyleBarX", "StyleBarY", "ActiveBarX", "ActiveBarY",
                   "ExternalSB", "UnlimitedWorld", "PosAutoBarX", "PosAutoBarY",
                   "RectBarX", "RectBarY", "MoveWorld"))


def inventory(game_root: Path, m25_inventory: dict) -> dict:
    nodes = list(m23.nodes(m23.parse_blocks(decode_dat(game_root / "CFG/Main.dat"))))
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in nodes if node.path.startswith("ML/Style/")}
    gi = m25_inventory["gi"]["resources"]
    cache, available = m24.image_sources(game_root)
    records = []
    for node in nodes:
        if node.name != "PanelScrollBar" or node.path.startswith("ML/Style/"):
            continue
        effective = m23.effective(m23.style_chain(node, styles))
        props = {key: values[-1] for key, values in effective.items()}
        bars = {}
        for axis in ("X", "Y"):
            style_key = props.get("StyleBar" + axis, "")
            active = props.get("ActiveBar" + axis, "False").casefold() == "true"
            style = styles.get(style_key)
            style_props = ({key: values[-1] for key, values in
                            m23.effective(m23.style_chain(style, styles)).items()}
                           if style else {})
            images = []
            for name in IMAGE_NAMES:
                if name not in style_props:
                    continue
                mode, sep, key = style_props[name].partition(",")
                if not sep:
                    mode, key = "Simple", mode
                resource = gi.get(key, {}) if mode == "GI" else {}
                resolved = m24.resource_resolved(key, cache, available)
                images.append({"property": name, "mode": mode, "key": key,
                               "status": resource.get("status", "UNKNOWN"),
                               "dimensions": resource.get("dimensions"),
                               "resolved": resolved})
            bars[axis] = {"active": active, "style": style_key,
                          "style_resolved": bool(style), "images": images,
                          "render_eligible": not active or
                          (bool(style) and bool(images) and
                           all(image["resolved"] for image in images))}
        unknown = sorted(set(props) - BASE - PANEL)
        eligible = all(bar["render_eligible"] for bar in bars.values()) and not unknown
        records.append({"path": node.unique_path, "name": props.get("Name", ""),
                        "size": props.get("Size", ""),
                        "external": props.get("ExternalSB", "False"),
                        "unlimited_world": props.get("UnlimitedWorld", "True"),
                        "move_world": props.get("MoveWorld", "False"),
                        "rect_x": props.get("RectBarX", ""),
                        "rect_y": props.get("RectBarY", ""),
                        "auto_x": props.get("PosAutoBarX", "True"),
                        "auto_y": props.get("PosAutoBarY", "True"),
                        "bars": bars, "unknown_properties": unknown,
                        "render_eligible": eligible,
                        "interactive_eligible": False,
                        "category": "RENDER_ONLY" if eligible else "UNSUPPORTED"})
    eligible = [record for record in records if record["render_eligible"]]
    return {"read_only": True, "physical": len(records),
            "external": sum(record["external"] == "True" for record in records),
            "active_vertical": sum(record["bars"]["Y"]["active"] for record in records),
            "style_y": dict(sorted(Counter(record["bars"]["Y"]["style"]
                                         for record in records).items())),
            "render_eligible": len(eligible),
            "deterministic_candidate": min(eligible, key=lambda item: item["path"].casefold())
                                       if eligible else None,
            "controls": records}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    parser.add_argument("m25_inventory", type=Path)
    args = parser.parse_args()
    prior = json.loads(args.m25_inventory.read_text(encoding="utf-8-sig"))
    print(json.dumps(inventory(args.game_root, prior), ensure_ascii=False, indent=2))
