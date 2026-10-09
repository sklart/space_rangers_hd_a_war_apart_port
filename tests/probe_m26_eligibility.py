#!/usr/bin/env python3
"""Recount release UI subtrees after the M26 render-only controls."""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path

import probe_m23_text_release as m23
from probe_ui_images_release import KNOWN_CONTROLS, decode_dat


NEW_TYPES = ("GraphBuf", "ScrollBar", "PanelScrollBar", "Edit")
VISUAL = {"Image", "SimpleImage", "TransImage", "AlphaImage", "GI", "GAI",
          "GraphButton", "Window", "Label", *NEW_TYPES}


def inventory(root: Path, prior: dict, new: dict[str, dict]) -> dict:
    parsed = m23.parse_blocks(decode_dat(root / "CFG/Main.dat"))
    nodes = [node for node in m23.nodes(parsed) if node.name in KNOWN_CONTROLS
             and not node.path.startswith("ML/Style/")]
    old = {row["path"]: row for row in prior["after_gai"]["controls"]}
    added = {row["path"]: row for kind in NEW_TYPES for row in new[kind]["controls"]}

    def eligible(node: m23.Node) -> bool:
        row = added.get(node.unique_path) or old.get(node.unique_path)
        return bool(row and (row.get("render_eligible") or row.get("category") == "RENDER_ONLY"))

    candidates = []
    blockers = Counter()
    for panel in (node for node in nodes if node.name in ("Panel", "Window", "PanelScrollBar")):
        descendants = [panel, *(node for node in m23.nodes(panel)
                                  if node.name in KNOWN_CONTROLS)]
        depth = max((node.unique_path.count("/") - panel.unique_path.count("/") + 1
                     for node in descendants), default=0)
        visual = sum(node.name in VISUAL for node in descendants)
        if depth < 2 or visual < 3:
            continue
        bad = {node.name for node in descendants if not eligible(node)}
        if bad:
            blockers.update(bad)
            continue
        candidates.append({"path": panel.unique_path, "nodes": len(descendants),
                           "max_depth": depth, "visual_leaves": visual,
                           "control_types": dict(sorted(Counter(node.name for node in descendants).items()))})
    candidates.sort(key=lambda row: (-row["nodes"], -row["visual_leaves"], row["path"]))
    return {"read_only": True, "candidate_count": len(candidates),
            "largest": candidates[0] if candidates else None,
            "top": candidates[:30], "blocker_root_counts": dict(blockers.most_common()),
            "eligible_controls": dict(sorted(Counter(node.name for node in nodes if eligible(node)).items()))}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    parser.add_argument("m25_inventory", type=Path)
    parser.add_argument("graphbuf", type=Path)
    parser.add_argument("scrollbar", type=Path)
    parser.add_argument("panel_scrollbar", type=Path)
    parser.add_argument("edit", type=Path)
    args = parser.parse_args()
    load = lambda path: json.loads(path.read_text(encoding="utf-8-sig"))
    print(json.dumps(inventory(args.game_root, load(args.m25_inventory),
                               dict(zip(NEW_TYPES, map(load, (args.graphbuf, args.scrollbar,
                                                              args.panel_scrollbar, args.edit))))),
                     ensure_ascii=False, indent=2))
