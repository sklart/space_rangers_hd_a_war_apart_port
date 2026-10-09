#!/usr/bin/env python3
"""Recount release UI subtrees after the M26 render-only controls."""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path

import probe_m23_text_release as m23
import probe_m24_controls_release as m24
from probe_ui_images_release import KNOWN_CONTROLS, decode_dat


NEW_TYPES = ("GraphBuf", "ScrollBar", "PanelScrollBar", "Edit")
VISUAL = {"Image", "SimpleImage", "TransImage", "AlphaImage", "GI", "GAI",
          "GraphButton", "Window", "Label", *NEW_TYPES}


def inventory(root: Path, prior: dict, new: dict[str, dict],
              classify_image_roots: bool = True,
              image_sources: tuple[dict[str, str], set[str]] | None = None) -> dict:
    parsed = m23.parse_blocks(decode_dat(root / "CFG/Main.dat"))
    all_nodes = list(m23.nodes(parsed))
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in all_nodes if node.path.startswith("ML/Style/")}
    nodes = [node for node in all_nodes if node.name in KNOWN_CONTROLS
             and not node.path.startswith("ML/Style/")]
    old = {row["path"]: row for row in prior["after_gai"]["controls"]}
    added = {row["path"]: row for kind in NEW_TYPES
             for row in new.get(kind, {}).get("controls", [])}

    def eligible(node: m23.Node) -> bool:
        row = added.get(node.unique_path) or old.get(node.unique_path)
        return bool(row and (row.get("render_eligible") or row.get("category") == "RENDER_ONLY"))

    candidates = []
    blockers = Counter()
    image_only_roots = 0
    image_only_causes = Counter()
    cache, available = image_sources if image_sources is not None else (None, None)
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
            if classify_image_roots and bad == {"Image"}:
                image_only_roots += 1
                if cache is None:
                    cache, available = m24.image_sources(root)
                causes = set()
                for image in (item for item in descendants
                              if item.name == "Image" and not eligible(item)):
                    mode, key = m23.image_reference(image, styles)
                    if not key:
                        causes.add("dynamic_or_missing_key")
                    elif not m24.resource_resolved(key, cache, available):
                        causes.add("unresolved_" + mode.lower())
                    elif mode == "GI":
                        causes.add("gi_format_or_render_semantics")
                    elif mode == "GAI":
                        causes.add("generic_gai")
                    elif mode in ("Anim", "AImage"):
                        causes.add("animation")
                    elif mode == "GraphBuf":
                        causes.add("graphbuf")
                    else:
                        causes.add("other_" + mode.lower())
                image_only_causes.update(causes)
            continue
        candidates.append({"path": panel.unique_path, "nodes": len(descendants),
                           "max_depth": depth, "visual_leaves": visual,
                           "control_types": dict(sorted(Counter(node.name for node in descendants).items()))})
    candidates.sort(key=lambda row: (-row["nodes"], -row["visual_leaves"],
                                     -row["max_depth"], row["path"]))
    deterministic = min(candidates, key=lambda row: row["path"]) if candidates else None
    diverse = min(candidates, key=lambda row: (-len(row["control_types"]),
                                               -row["nodes"], -row["visual_leaves"],
                                               -row["max_depth"], row["path"])) if candidates else None
    return {"read_only": True, "candidate_count": len(candidates),
            "largest": candidates[0] if candidates else None,
            "deterministic": deterministic, "most_diverse": diverse,
            "top": candidates[:30], "blocker_root_counts": dict(blockers.most_common()),
            "image_only_roots": image_only_roots,
            "image_only_root_causes_overlapping": dict(image_only_causes.most_common()),
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
    prior = load(args.m25_inventory)
    all_new = dict(zip(NEW_TYPES, map(load, (args.graphbuf, args.scrollbar,
                                          args.panel_scrollbar, args.edit))))
    sources = m24.image_sources(args.game_root)
    stages = {}
    for through in range(len(NEW_TYPES) + 1):
        stage = inventory(args.game_root, prior,
                          {kind: all_new[kind] for kind in NEW_TYPES[:through]},
                          classify_image_roots=through == 0,
                          image_sources=sources)
        stages["M25" if through == 0 else "+".join(NEW_TYPES[:through])] = {
            "candidate_count": stage["candidate_count"], "largest": stage["largest"],
            **({"image_only_roots": stage["image_only_roots"],
                "image_only_root_causes_overlapping":
                    stage["image_only_root_causes_overlapping"]} if through == 0 else {})}
    result = inventory(args.game_root, prior, all_new, image_sources=sources)
    result["stage_progression"] = stages
    print(json.dumps(result,
                     ensure_ascii=False, indent=2))
