#!/usr/bin/env python3
"""Read-only physical ScrollBar inventory from licensed Main.dat and M25 GI probe."""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path

import probe_m23_text_release as m23
from probe_ui_images_release import decode_dat


IMAGE_PREFIXES = ("ImageUp", "ImageBar", "ImageTop", "ImageCenter",
                  "ImageBottom", "ImageDown")
IMAGE_NAMES = tuple(prefix + suffix for prefix in IMAGE_PREFIXES
                    for suffix in ("N", "A", "D"))
BASE = frozenset(("Style", "Pos", "PosZ", "Size", "Sme", "Name", "Active",
                  "Help", "MVUpdate", "MouseBlocking", "MouseBlockingTest"))
SCROLL = frozenset(("Min", "Max", "PageSize", "LargeChange", "SmallChange",
                    "Position", "Kind", "KindCalc", *IMAGE_NAMES))


def integers(props: dict[str, str], keys: tuple[str, ...]) -> dict[str, int] | None:
    try:
        return {key: int(props[key]) for key in keys if key in props}
    except ValueError:
        return None


def inventory(game_root: Path, m25_inventory: dict) -> dict:
    nodes = list(m23.nodes(m23.parse_blocks(decode_dat(game_root / "CFG/Main.dat"))))
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in nodes if node.path.startswith("ML/Style/")}
    gi_resources = m25_inventory["gi"]["resources"]
    records = []
    for node in nodes:
        if node.name != "ScrollBar" or node.path.startswith("ML/Style/"):
            continue
        effective = m23.effective(m23.style_chain(node, styles))
        props = {key: values[-1] for key, values in effective.items()}
        numerical = integers(props, ("Min", "Max", "PageSize", "LargeChange",
                                     "SmallChange", "Position", "KindCalc"))
        images = {}
        for name in IMAGE_NAMES:
            if name not in props:
                continue
            value = props[name]
            mode, separator, key = value.partition(",")
            if not separator:
                mode, key = "Simple", value
            source = gi_resources.get(key, {}) if mode == "GI" else {}
            images[name] = {"mode": mode, "key": key,
                            "source_status": source.get("status", "UNKNOWN"),
                            "source_size": source.get("source_size"),
                            "dimensions": source.get("dimensions"),
                            "resolved": source.get("status") in ("Format0", "Format2")}
        unknown = sorted(set(props) - BASE - SCROLL)
        render_eligible = (numerical is not None and props.get("Kind", "y") in ("x", "y")
                           and not unknown and bool(images)
                           and all(image["resolved"] for image in images.values()))
        active = props.get("Active", "True").casefold() != "false"
        records.append({"path": node.unique_path, "style": props.get("Style", ""),
                        "active": active, "size": props.get("Size", ""),
                        "orientation": props.get("Kind", "y"),
                        "numeric": numerical, "images": images,
                        "unknown_properties": unknown,
                        "render_eligible": render_eligible,
                        "interactive_eligible": False,
                        "category": "RENDER_ONLY" if render_eligible else "UNSUPPORTED"})
    eligible = [record for record in records if record["render_eligible"]]
    return {"read_only": True, "physical": len(records),
            "styles": dict(sorted(Counter(record["style"] for record in records).items())),
            "orientations": dict(sorted(Counter(record["orientation"] for record in records).items())),
            "kinds_calc": dict(sorted(Counter(str(record["numeric"].get("KindCalc"))
                                              if record["numeric"] else "INVALID"
                                              for record in records).items())),
            "image_modes": dict(sorted(Counter(image["mode"] for record in records
                                               for image in record["images"].values()).items())),
            "render_eligible": len(eligible),
            "active_render_eligible": sum(record["active"] for record in eligible),
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
