#!/usr/bin/env python3
"""Read-only release subtree eligibility after raw GI and supported GAI subset."""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re

import probe_m23_text_release as m23
import probe_m24_controls_release as m24
from probe_ui_images_release import KNOWN_CONTROLS, decode_dat

BASE_METADATA = frozenset(("Help", "MVUpdate", "MouseBlocking", "MouseBlockingTest"))
FRAME_RANGE = re.compile(r"\s*\[\s*(\d+)\s*,\s*(\d+)\s*-\s*(\d+)\s*\]")


def inventory(root: Path, source: dict) -> dict:
    parsed = m23.parse_blocks(decode_dat(root / "CFG/Main.dat"))
    all_nodes = list(m23.nodes(parsed))
    styles = {"Style." + n.path.removeprefix("ML/Style/").replace("/", "."): n
              for n in all_nodes if n.path.startswith("ML/Style/")}
    controls = [n for n in all_nodes if n.name in KNOWN_CONTROLS
                and not n.path.startswith("ML/Style/")]
    effective = {id(n): m23.effective(m23.style_chain(n, styles)) for n in controls}
    cache, available = m24.image_sources(root)
    translations = m23.language_map(root / "CFG/Rus/Lang.dat")
    fonts = m23.font_sources(root, {m24.final(effective[id(n)], "Font") for n in controls
                                    if n.name in ("Label", "GraphButton") and
                                    m24.final(effective[id(n)], "Font")})
    gai = {c["path"]: c for c in source["gai"]["controls"]}

    def image_ok(raw: str, stage: str) -> bool:
        mode, key = m24.mode_and_key(raw)
        if not m24.resource_resolved(key, cache, available):
            return False
        if mode == "GI":
            return source["gi"]["resources"].get(key, {}).get("status") in ("Format0", "Format2")
        if mode == "GAI":
            return False  # generic GAI keys require their own container inventory
        return mode in m24.PORTABLE_IMAGE_MODES

    def gai_ok(node: m23.Node) -> bool:
        record = gai.get(node.unique_path, {})
        props = record.get("properties", {})
        container = record.get("container", {})
        if (not record.get("resolved") or "PBuf" in props or "ImageFirst" in props or
                container.get("status") != "valid" or container.get("flags") != 0 or
                not container.get("frame_formats") or
                any(fmt not in ("0", "2") for fmt in container["frame_formats"])):
            return False
        if "FrameLoad" in props:
            try: index = int(props["FrameLoad"][-1])
            except ValueError: return False
            if not 0 <= index < container["sequence_count"]: return False
        elif "Frame" in props:
            frame = props["Frame"][-1]
            at = 0
            while at < len(frame):
                match = FRAME_RANGE.match(frame, at)
                if not match: return False
                delay, first, last = map(int, match.groups())
                if delay <= 0 or first >= container["frame_count"] or last >= container["frame_count"]:
                    return False
                at = match.end()
        return True

    def render_ok(node: m23.Node, stage: str) -> bool:
        props = effective[id(node)]
        if node.name == "Panel": return True
        if node.name == "Zone":
            return set(props) <= m24.ZONE_PROPERTIES | BASE_METADATA and \
                   m24.final(props, "Kind", "Rect") in ("Rect", "Circle")
        if node.name == "GraphButton":
            if not set(props) <= m24.BUTTON_PROPERTIES | BASE_METADATA: return False
            if m24.final(props, "KindHit", "Rect") not in ("Rect", "Graph", "ImageHit"): return False
            if m24.final(props, "Caption") and not fonts.get(m24.final(props, "Font")): return False
            return all(image_ok(raw, stage) for slot in m24.BUTTON_SLOTS for raw in props.get(slot, []))
        if node.name == "Window":
            return set(props) <= m24.WINDOW_PROPERTIES | BASE_METADATA and \
                   all(image_ok(raw, stage) for slot in m24.WINDOW_SLOTS
                       for raw in props.get(slot, [])) and \
                   all(props.get(slot) for slot in m24.WINDOW_SLOTS)
        if node.name in m24.IMAGE_TYPES:
            mode, key = m23.image_reference(node, styles)
            return image_ok(f"{mode},{key}", stage)
        if node.name == "Label":
            font = fonts.get(m24.final(props, "Font"), {})
            if not font or not set(props) <= m23.LABEL_PROPERTIES | BASE_METADATA: return False
            lines = props.get("Text", [])
            text = "\n".join(translations.get("\n".join(lines).strip(), lines))
            found, unknown = m23.tags(text)
            if unknown or "object" in found: return False
            if any(ord(c) not in font["codes"] for c in m23.visible_text(text) if c not in "\r\n"):
                return False
            image = m24.final(props, "Image")
            return not image or image_ok(image, stage)
        if node.name == "GAI": return stage == "after_gai" and gai_ok(node)
        return False

    result = {}
    for stage in ("after_gi", "after_gai"):
        candidates = []
        blockers = Counter()
        eligible_controls = Counter()
        for node in controls:
            if render_ok(node, stage): eligible_controls[node.name] += 1
        for panel in (n for n in controls if n.name in ("Panel", "Window")):
            descendants = [panel, *(n for n in m23.nodes(panel) if n.name in KNOWN_CONTROLS)]
            depth = max((n.unique_path.count("/") - panel.unique_path.count("/") + 1
                         for n in descendants), default=0)
            visual = sum(n.name in m24.VISUAL_TYPES or (stage == "after_gai" and n.name == "GAI")
                         for n in descendants)
            if depth < 2 or visual < 3: continue
            bad = [n.name for n in descendants if not render_ok(n, stage)]
            if bad:
                blockers.update(set(bad))
                continue
            candidates.append({"path": panel.unique_path, "nodes": len(descendants),
                               "max_depth": depth, "visual_leaves": visual,
                               "control_types": dict(sorted(Counter(n.name for n in descendants).items()))})
        selected = min(candidates, key=lambda row: row["path"].casefold()) if candidates else None
        largest = max(candidates, key=lambda row: (row["nodes"], row["path"])) if candidates else None
        result[stage] = {"found": bool(candidates), "candidate_count": len(candidates),
                         "selected": selected, "largest": largest,
                         "most_common_root": Counter(c["path"].split("/")[1] for c in candidates).most_common(1),
                         "eligible_controls": dict(sorted(eligible_controls.items())),
                         "blocker_root_counts": dict(sorted(blockers.items()))}
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    parser.add_argument("release_inventory", type=Path)
    args = parser.parse_args()
    source = json.loads(args.release_inventory.read_text(encoding="utf-8-sig"))
    print(json.dumps(inventory(args.game_root, source), ensure_ascii=False, indent=2))
