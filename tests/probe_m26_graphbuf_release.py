#!/usr/bin/env python3
"""Read-only physical GraphBuf inventory from the licensed release Main.dat."""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path

import probe_m23_text_release as m23
import probe_m24_controls_release as m24
from probe_ui_images_release import decode_dat


BASE = frozenset(("Style", "Pos", "PosZ", "Size", "Sme", "Name", "Active",
                  "Help", "MVUpdate", "MouseBlocking", "MouseBlockingTest"))
GRAPHBUF = frozenset(("HalfAlpha", "CacheRGBA", "CacheGI"))


def pair(value: str) -> tuple[int, int] | None:
    try:
        parts = value.split(",")
        if len(parts) != 2:
            return None
        return int(parts[0]), int(parts[1])
    except ValueError:
        return None


def aspect_fit(source: tuple[int, int], target: tuple[int, int]) -> tuple[int, int] | None:
    sw, sh = source
    tw, th = target
    if min(sw, sh, tw, th) <= 0:
        return None
    if sw * th >= sh * tw:
        return tw, max(1, round(tw / sw * sh))
    return max(1, round(th / sh * sw)), th


def inventory(game_root: Path, m25_inventory: dict) -> dict:
    all_nodes = list(m23.nodes(m23.parse_blocks(decode_dat(game_root / "CFG/Main.dat"))))
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in all_nodes if node.path.startswith("ML/Style/")}
    physical = [node for node in all_nodes if node.name == "GraphBuf"
                and not node.path.startswith("ML/Style/")]
    cache, available = m24.image_sources(game_root)
    gi = m25_inventory["gi"]["resources"]
    records = []
    for node in physical:
        effective = m23.effective(m23.style_chain(node, styles))
        props = {key: values[-1] for key, values in effective.items()}
        kind = "CacheRGBA" if "CacheRGBA" in props else "CacheGI" if "CacheGI" in props else "dynamic"
        key = props.get(kind, "") if kind != "dynamic" else ""
        resolved = m24.resource_resolved(key, cache, available) if key else False
        source = gi.get(key, {}) if kind == "CacheGI" else {}
        source_size = source.get("source_size")
        source_format = source.get("status")
        source_dimensions = source.get("dimensions")
        target = pair(props.get("Size", ""))
        scaled = aspect_fit(tuple(source_dimensions), target) if source_dimensions and target else None
        unknown = sorted(set(props) - BASE - GRAPHBUF)
        render_eligible = (kind != "dynamic" and resolved and target is not None
                           and min(target) > 0 and not unknown and
                           (kind != "CacheGI" or source_format in ("Format0", "Format2")))
        records.append({"path": node.unique_path, "style": props.get("Style", ""),
                        "size": target, "source_kind": kind, "source_key": key,
                        "resolved": resolved, "source_format": source_format,
                        "source_size": source_size, "source_dimensions": source_dimensions,
                        "scaled_dimensions": scaled, "half_alpha_declared": "HalfAlpha" in props,
                        "half_alpha": props.get("HalfAlpha", "False"),
                        "other_properties": unknown,
                        "render_eligible": render_eligible,
                        "interaction_eligible": False,
                        "category": "RENDER_ONLY" if render_eligible else "UNSUPPORTED",
                        "reason": "dynamic buffer has no config-backed source" if kind == "dynamic"
                                  else "unsupported resource or instance properties" if not render_eligible
                                  else "input/event layer deferred"})
    eligible = [record for record in records if record["render_eligible"]]
    return {"read_only": True, "physical": len(records),
            "styles": dict(sorted(Counter(record["style"] for record in records).items())),
            "source_kinds": dict(sorted(Counter(record["source_kind"] for record in records).items())),
            "half_alpha_declared": sum(record["half_alpha_declared"] for record in records),
            "half_alpha_enabled": sum(record["half_alpha"].casefold() == "true" for record in records),
            "resolved_keys": sorted({record["source_key"] for record in records if record["resolved"]}),
            "unresolved_keys": sorted({record["source_key"] for record in records
                                       if record["source_key"] and not record["resolved"]}),
            "render_eligible": len(eligible),
            "deterministic_candidate": min(eligible, key=lambda record: record["path"].casefold()) if eligible else None,
            "largest_area_candidate": max(eligible, key=lambda record: record["size"][0] * record["size"][1]) if eligible else None,
            "controls": records}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    parser.add_argument("m25_inventory", type=Path)
    args = parser.parse_args()
    m25 = json.loads(args.m25_inventory.read_text(encoding="utf-8-sig"))
    print(json.dumps(inventory(args.game_root, m25), ensure_ascii=False, indent=2))
