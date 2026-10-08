#!/usr/bin/env python3
"""Read-only M24 control inventory with instance-level resource eligibility.

The M23 blocker occurrence counts repeat one physical control in every
overlapping candidate subtree.  This probe reports physical config instances
separately from those blocker counts.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import json
import pathlib

import probe_m23_text_release as m23
import probe_m22_ui_release as m22
from probe_ui_images_release import KNOWN_CONTROLS, data_entries, decode_dat, entries

BUTTON_SLOTS = ("ImageNormal", "ImageNormalA", "ImageDown", "ImageDownA",
                "ImageDisable", "ImageDisableA", "ImageHit")
WINDOW_SLOTS = ("ImageTopLeft", "ImageTopRight", "ImageBottomLeft",
                "ImageBottomRight", "ImageLeft", "ImageRight", "ImageTop",
                "ImageBottom", "ImageTexture")
PORTABLE_IMAGE_MODES = frozenset(("Simple", "Trans", "Alpha"))
BASE = frozenset(("Style", "Pos", "PosZ", "Size", "Sme", "Name", "Active"))
BUTTON_PROPERTIES = BASE | frozenset((
    "Kind", "KindHit", "Auto", "Disable", "Down", "UpOnlyDown", "Font",
    "Caption", "CaptionColor", "CaptionShadow", "CaptionAlignX",
    "CaptionAlignY", "CaptionSme", "SoundEnter", "SoundLeave", "SoundClick",
    *("CaptionColor" + state for state in
      ("Normal", "NormalA", "Down", "DownA", "Disable", "DisableA")),
    *("CaptionShadowColor" + state for state in
      ("Normal", "NormalA", "Down", "DownA", "Disable", "DisableA")),
    *(slot for slot in BUTTON_SLOTS),
    *(slot + "_Pos" for slot in BUTTON_SLOTS),
))
WINDOW_PROPERTIES = BASE | frozenset((*WINDOW_SLOTS, "WorkSubRect", "MinSize"))
ZONE_PROPERTIES = BASE | frozenset(("Kind",))
IMAGE_TYPES = frozenset(("Image", "SimpleImage", "TransImage", "AlphaImage"))
VISUAL_TYPES = IMAGE_TYPES | frozenset(("Label", "GraphButton", "Window"))


def mode_and_key(raw: str) -> tuple[str, str]:
    parts = [part.strip() for part in raw.split(",", 1)]
    return (parts[0], parts[1]) if len(parts) == 2 else ("Simple", parts[0])


def final(props: dict[str, list[str]], key: str, default: str = "") -> str:
    return props.get(key, [default])[-1]


def image_sources(game_root: pathlib.Path) -> tuple[dict[str, str], set[str]]:
    cache = {path.replace("/", ".").casefold(): name.replace("\\", "/").casefold()
             for path, name in data_entries(decode_dat(game_root / "CFG/CacheData.dat", 0xEA8F3F37))}
    available: set[str] = set()
    for package in sorted((game_root / "DATA").glob("*.pkg")):
        available.update(item[0].replace("\\", "/").casefold()
                         for item in entries(package.read_bytes()))
    return cache, available


def image_eligibility(raw: str, cache: dict[str, str], available: set[str]) -> tuple[str, str, str]:
    mode, key = mode_and_key(raw)
    if mode not in PORTABLE_IMAGE_MODES:
        return mode, key, "unsupported image mode " + mode
    if not key:
        return mode, key, "empty image key"
    if not resource_resolved(key, cache, available):
        return mode, key, "missing backing resource"
    return mode, key, ""


def resource_resolved(key: str, cache: dict[str, str], available: set[str]) -> bool:
    normalized = key.replace("\\", "/").casefold()
    return bool(key) and cache.get(normalized, normalized) in available


def inventory(game_root: pathlib.Path, language: str = "Rus") -> dict:
    main = decode_dat(game_root / "CFG/Main.dat")
    all_nodes = list(m23.nodes(m23.parse_blocks(main)))
    runtime = [node for node in all_nodes if not node.path.startswith("ML/Style/")]
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in all_nodes if node.path.startswith("ML/Style/")}
    cache, available = image_sources(game_root)
    translations = m23.language_map(game_root / "CFG" / language / "Lang.dat")
    effective = {id(node): m23.effective(m23.style_chain(node, styles))
                 for node in runtime if node.name in KNOWN_CONTROLS}
    controls = [node for node in runtime if node.name in KNOWN_CONTROLS]
    buttons = [node for node in controls if node.name == "GraphButton"]
    windows = [node for node in controls if node.name == "Window"]
    zones = [node for node in controls if node.name == "Zone"]
    fonts = m23.font_sources(game_root, {final(effective[id(node)], "Font")
                                         for node in controls if node.name in ("Label", "GraphButton")
                                         and final(effective[id(node)], "Font")})
    eligibility: dict[int, list[str]] = {}

    def images(node: m23.Node, slots: tuple[str, ...]) -> tuple[list[dict], list[str]]:
        props = effective[id(node)]
        records: list[dict] = []
        reasons: list[str] = []
        for slot in slots:
            for raw in props.get(slot, []):
                mode, key, reason = image_eligibility(raw, cache, available)
                records.append({"slot": slot, "raw": raw, "mode": mode,
                                "key": key, "resource_resolved": resource_resolved(key, cache, available)})
                if reason:
                    reasons.append(f"{slot}: {reason}")
        return records, reasons

    def summarize(nodes: list[m23.Node], slots: tuple[str, ...], allowed: frozenset[str]) -> dict:
        modes: Counter[str] = Counter()
        slots_used: Counter[str] = Counter()
        resource_keys: Counter[str] = Counter()
        resolved = 0
        unresolved = 0
        properties: Counter[str] = Counter()
        styles_used: Counter[str] = Counter()
        reason_counts: Counter[str] = Counter()
        examples: list[dict] = []
        eligible_paths: list[str] = []
        for node in nodes:
            props = effective[id(node)]
            records, reasons = images(node, slots)
            modes.update(record["mode"] for record in records)
            slots_used.update(record["slot"] for record in records)
            resource_keys.update(record["key"] for record in records if record["key"])
            resolved += sum(record["resource_resolved"] for record in records)
            unresolved += sum(not record["resource_resolved"] for record in records)
            properties.update(props.keys())
            styles_used[node.last("Style", "(none)")] += 1
            reasons.extend("unsupported property " + key for key in props if key not in allowed)
            if node.name == "GraphButton":
                if final(props, "Kind", "Normal") not in ("Normal", "Fix", "Disable", "FixDisable"):
                    reasons.append("unknown Kind")
                if final(props, "KindHit", "Rect") not in ("Rect", "Graph", "ImageHit"):
                    reasons.append("unknown KindHit")
                if final(props, "Caption") and not fonts.get(final(props, "Font")):
                    reasons.append("caption font unresolved")
                if final(props, "KindHit") == "ImageHit" and not final(props, "ImageHit"):
                    reasons.append("ImageHit slot missing")
            # Unknown child controls are still inspected separately by subtree search.
            reasons = sorted(set(reasons))
            eligibility[id(node)] = reasons
            if reasons:
                reason_counts.update(reasons)
                if len(examples) < 8:
                    examples.append({"path": node.unique_path, "reasons": reasons})
            else:
                eligible_paths.append(node.unique_path)
        return {"physical_occurrences": len(nodes), "eligible": len(eligible_paths),
                "ineligible": len(nodes) - len(eligible_paths),
                "slot_counts": dict(sorted(slots_used.items())),
                "image_modes": dict(sorted(modes.items())),
                "resource_resolved": resolved, "resource_unresolved": unresolved,
                "distinct_resource_keys": len(resource_keys),
                "property_counts": dict(sorted(properties.items())),
                "styles": dict(sorted(styles_used.items())),
                "ineligible_reasons": dict(sorted(reason_counts.items())),
                "ineligible_examples": examples,
                "first_eligible_path": min(eligible_paths, key=str.casefold) if eligible_paths else "NOT FOUND"}

    graph = summarize(buttons, BUTTON_SLOTS, BUTTON_PROPERTIES)
    window = summarize(windows, WINDOW_SLOTS, WINDOW_PROPERTIES)
    zone_kinds: Counter[str] = Counter()
    zone_styles: Counter[str] = Counter()
    zone_reasons: Counter[str] = Counter()
    for node in zones:
        props = effective[id(node)]
        kind = final(props, "Kind", "Rect")
        zone_kinds[kind] += 1
        zone_styles[node.last("Style", "(none)")] += 1
        reasons = []
        if kind not in ("Rect", "Circle"):
            reasons.append("unknown Kind")
        reasons.extend("unsupported property " + key for key in props if key not in ZONE_PROPERTIES)
        eligibility[id(node)] = reasons
        zone_reasons.update(reasons)

    def resource_ok(node: m23.Node) -> bool:
        props = effective[id(node)]
        if node.name == "Panel" or node.name == "Zone":
            return not eligibility.get(id(node))
        if node.name in ("GraphButton", "Window"):
            return not eligibility.get(id(node))
        if node.name in IMAGE_TYPES:
            mode, key = m23.image_reference(node, styles)
            return not image_eligibility(f"{mode},{key}", cache, available)[2]
        if node.name == "Label":
            font = fonts.get(final(props, "Font"), {})
            if not font or not set(props) <= m23.LABEL_PROPERTIES:
                return False
            values = props.get("Text", [])
            text = "\n".join(translations.get("\n".join(values).strip(), values))
            found, unknown = m23.tags(text)
            if unknown or "object" in found:
                return False
            if any(ord(c) not in font["codes"] for c in m23.visible_text(text) if c not in "\r\n"):
                return False
            image = final(props, "Image")
            return not image or not image_eligibility(image, cache, available)[2]
        return False

    scenarios = {
        "M23": frozenset(), "GraphButton": frozenset(("GraphButton",)),
        "Window": frozenset(("Window",)), "Zone": frozenset(("Zone",)),
        "GraphButton+Window": frozenset(("GraphButton", "Window")),
        "GraphButton+Zone": frozenset(("GraphButton", "Zone")),
        "All M24": frozenset(("GraphButton", "Window", "Zone")),
    }
    baseline = m23.SUPPORTED
    subtree_counts: dict[str, int] = {}
    candidates: dict[str, list[dict]] = {}
    type_blocker_roots: Counter[str] = Counter()
    type_blocker_occurrences: Counter[str] = Counter()
    eligibility_blocker_roots: Counter[str] = Counter()
    eligibility_blocker_occurrences: Counter[str] = Counter()
    for scenario, added in scenarios.items():
        supported = baseline | added
        found: list[dict] = []
        for root in (n for n in controls if n.name in ("Panel", "Window")):
            descendants = [root, *(n for n in m23.nodes(root) if n.name in KNOWN_CONTROLS)]
            depth = max((n.unique_path.count("/") - root.unique_path.count("/") + 1
                         for n in descendants), default=0)
            visual = sum(n.name in VISUAL_TYPES for n in descendants)
            if depth < 2 or visual < 3:
                continue
            type_blockers = [n.name for n in descendants if n.name not in supported]
            eligibility_blockers = [n.name for n in descendants
                                    if n.name in supported and not resource_ok(n)]
            if type_blockers or eligibility_blockers:
                if scenario == "All M24":
                    type_blocker_roots.update(set(type_blockers))
                    type_blocker_occurrences.update(type_blockers)
                    eligibility_blocker_roots.update(set(eligibility_blockers))
                    eligibility_blocker_occurrences.update(eligibility_blockers)
                continue
            found.append({"path": root.unique_path, "nodes": len(descendants),
                          "max_depth": depth, "visual_leaves": visual,
                          "control_types": dict(sorted(Counter(n.name for n in descendants).items()))})
        subtree_counts[scenario] = len(found)
        candidates[scenario] = found
    selected = min(candidates["All M24"], key=lambda c: c["path"].casefold()) if candidates["All M24"] else None
    legacy_blocks, _ = m22.paths_for(main)
    unique_controls = [path for path in legacy_blocks if path.rsplit("/", 1)[-1] in KNOWN_CONTROLS]
    # A normalized path can represent repeated physical instances.  Count it as
    # release eligible only if every occurrence is eligible.
    by_path: dict[str, list[m23.Node]] = defaultdict(list)
    for node in controls:
        by_path[node.path].append(node)
    supported_unique = sum(path.rsplit("/", 1)[-1] in baseline for path in unique_controls)
    eligible_m23_unique = sum(path.rsplit("/", 1)[-1] in baseline and
                              bool(by_path.get(path)) and all(resource_ok(node) for node in by_path[path])
                              for path in unique_controls)
    eligible_new_unique = sum(name in ("GraphButton", "Window", "Zone") and
                              all(not eligibility.get(id(node)) for node in by_path.get(path, []))
                              for path in unique_controls
                              for name in [path.rsplit("/", 1)[-1]])
    return {
        "source": "CFG/Main.dat", "language": language,
        "read_only": True,
        "count_note": "Physical config occurrences differ from overlapping M23 candidate-subtree blocker counts.",
        "m23_blocker_occurrences": {"GraphButton": 1682, "Window": 68, "Zone": 255},
        "graph_button": {**graph,
                         "kind": dict(sorted(Counter(final(effective[id(n)], "Kind", "Normal") for n in buttons).items())),
                         "kind_hit": dict(sorted(Counter(final(effective[id(n)], "KindHit", "Rect") for n in buttons).items())),
                         "caption": sum(bool(final(effective[id(n)], "Caption")) for n in buttons),
                         "font": sum(bool(final(effective[id(n)], "Font")) for n in buttons),
                         "localized_caption": sum(final(effective[id(n)], "Caption") in translations for n in buttons),
                         "on_press_code": sum(any(c.name == "OnPressCode" for c in n.children) for n in buttons),
                         "configured_child_controls": sum(c.name in KNOWN_CONTROLS for n in buttons for c in n.children)},
        "window": {**window,
                   "min_size": sum("MinSize" in effective[id(n)] for n in windows),
                   "work_sub_rect": sum("WorkSubRect" in effective[id(n)] for n in windows),
                   "configured_child_controls": sum(c.name in KNOWN_CONTROLS for n in windows for c in n.children)},
        "zone": {"physical_occurrences": len(zones), "kind": dict(sorted(zone_kinds.items())),
                 "styles": dict(sorted(zone_styles.items())),
                 "eligible": sum(not eligibility[id(n)] for n in zones),
                 "ineligible_reasons": dict(sorted(zone_reasons.items()))},
        "unique_control_paths": {"total": len(unique_controls), "m23_supported_by_type": supported_unique,
                                 "m23_release_eligible": eligible_m23_unique,
                                 "new_m24_release_eligible": eligible_new_unique,
                                 "after_m24_release_eligible": eligible_m23_unique + eligible_new_unique},
        "hypothetical_unique_subtrees": subtree_counts,
        "fully_supported_real_subtree": selected or "NOT FOUND",
        "remaining_type_blockers": [{"control": name, "candidate_subtrees_blocked": count,
                                     "overlapping_occurrences": type_blocker_occurrences[name]}
                                    for name, count in type_blocker_roots.most_common()],
        "remaining_eligibility_blockers": [{"control": name, "candidate_subtrees_blocked": count,
                                            "overlapping_occurrences": eligibility_blocker_occurrences[name]}
                                           for name, count in eligibility_blocker_roots.most_common()],
    }


def self_test() -> None:
    assert mode_and_key("Bm.A") == ("Simple", "Bm.A")
    assert mode_and_key("Alpha, Bm.A") == ("Alpha", "Bm.A")
    assert image_eligibility("GI,Bm.A", {}, set())[2] == "unsupported image mode GI"
    assert not resource_resolved("Bm.A", {}, set())
    assert resource_resolved("Bm.A", {"bm.a": "data/a.png"}, {"data/a.png"})
    assert image_eligibility("Simple,Bm.A", {"bm.a": "data/a.png"}, {"data/a.png"})[2] == ""
    assert image_eligibility("Simple,Bm.A", {}, set())[2] == "missing backing resource"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", nargs="?", type=pathlib.Path)
    parser.add_argument("--language", default="Rus")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--json", type=pathlib.Path)
    args = parser.parse_args()
    if args.self_test:
        self_test()
        print("M24 CONTROL INVENTORY SELF-TEST PASS")
        return
    if args.game_root is None:
        parser.error("game_root is required")
    result = inventory(args.game_root, args.language)
    output = json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(output, encoding="utf-8")
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
