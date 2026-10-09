#!/usr/bin/env python3
"""Release-backed M27 interaction axes without claiming gameplay completion."""
from collections import Counter
from pathlib import Path
import json
import sys

import probe_m23_text_release as m23
import probe_m26_eligibility as m26
from probe_ui_images_release import KNOWN_CONTROLS, decode_dat


SCHEMA = ["path", "type", "m25_category", "m26_category", "render", "pointer", "focus",
          "keyboard", "action", "audio", "script", "m27_class"]
INPUT_TYPES = {"GraphButton", "Zone", "ScrollBar", "PanelScrollBar", "Edit"}


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def inventory(game_root: Path, project: Path):
    all_nodes = list(m23.nodes(m23.parse_blocks(decode_dat(game_root / "CFG/Main.dat"))))
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in all_nodes if node.path.startswith("ML/Style/")}
    nodes = [node for node in all_nodes if node.name in KNOWN_CONTROLS
             and not node.path.startswith("ML/Style/")]
    previous = {row["path"]: row for row in read(project / "m25-eligibility.json")["after_gai"]["controls"]}
    current = {row["path"]: row for filename in (
        "m26-graphbuf-inventory.json", "m26-scrollbar-inventory.json",
        "m26-panel-scrollbar-inventory.json", "m26-edit-inventory.json")
        for row in read(project / filename)["controls"]}
    assert len(nodes) == 3375
    rows = []
    lookup = {}
    for node in nodes:
        old = previous[node.unique_path]
        source = current.get(node.unique_path, old)
        render = bool(source.get("render_eligible") or source.get("category") == "RENDER_ONLY")
        pointer = render and node.name in INPUT_TYPES
        focus = render and node.name in ("ScrollBar", "Edit")
        keyboard = render and node.name == "Edit"
        action = "queue" if pointer else "none"
        audio = "none"
        script = "none"
        if pointer and node.name == "GraphButton":
            props = m23.effective(m23.style_chain(node, styles))
            if any(props.get(name) for name in ("SoundEnter", "SoundLeave", "SoundClick")):
                audio = "request_only"
            if any(props.get(name) for name in ("OnPressCode", "OnMouseEnterCode",
                                                "OnMouseLeaveCode", "OnMouseRightClick", "OnKey")):
                script = "request_only"
        if not render:
            category = "UNSUPPORTED"
        elif node.name == "GraphButton":
            category = "ACTION_DEFERRED"  # click emits an action; gameplay is external
        elif node.name == "Zone":
            category = "POINTER_READY"
        elif node.name in ("ScrollBar", "PanelScrollBar", "Edit"):
            category = "INPUT_READY"
        else:
            category = "RENDER_ONLY"
        row = [node.unique_path, node.name, old["category"], source["category"], render, pointer,
               focus, keyboard, action, audio, script, category]
        rows.append(row)
        lookup[node.unique_path] = row
    summary = {}
    for kind in sorted(INPUT_TYPES):
        subset = [row for row in rows if row[1] == kind]
        summary[kind] = {"physical": len(subset), "render": sum(row[4] for row in subset),
                         "pointer": sum(row[5] for row in subset),
                         "focus": sum(row[6] for row in subset),
                         "keyboard": sum(row[7] for row in subset),
                         "audio_request_only": sum(row[9] == "request_only" for row in subset),
                         "script_request_only": sum(row[10] == "request_only" for row in subset)}
    subtree = Counter()
    for panel in (node for node in nodes if node.name in ("Panel", "Window", "PanelScrollBar")):
        descendants = [panel, *(n for n in m23.nodes(panel) if n.name in KNOWN_CONTROLS)]
        depth = max((n.unique_path.count("/") - panel.unique_path.count("/") + 1
                     for n in descendants), default=0)
        visual = sum(n.name in m26.VISUAL for n in descendants)
        if depth < 2 or visual < 3 or any(not lookup[n.unique_path][4] for n in descendants):
            continue
        subtree["renderable"] += 1
        active = [lookup[n.unique_path] for n in descendants if lookup[n.unique_path][5]]
        if active: subtree["pointer_interactable"] += 1
        if any(row[6] for row in active): subtree["focus_interactable"] += 1
        if active and all(row[8] == "queue" and row[9] == "none" and row[10] == "none"
                          for row in active):
            subtree["portable_action_complete"] += 1
    assert subtree["renderable"] == 114, subtree
    return {"read_only": True, "source": "Main.dat + M25/M26 eligibility + M27 capability axes",
            "axes_schema": SCHEMA, "physical_controls": len(rows),
            "class_counts": dict(Counter(row[11] for row in rows)),
            "input_type_counts": summary, "subtree_counts": dict(subtree),
            "controls": rows}


def compact(result):
    old_codes = {name: number for number, name in
                 enumerate(sorted({row[index] for row in result["controls"]
                                   for index in (2, 3)}))}
    new_codes = {name: number for number, name in
                 enumerate(sorted({row[11] for row in result["controls"]}))}
    controls = []
    for path, _kind, m25, m26, render, pointer, focus, keyboard, action, audio, script, category in result["controls"]:
        bits = (int(render) | int(pointer) << 1 | int(focus) << 2 |
                int(keyboard) << 3 | int(action == "queue") << 4 |
                int(audio == "request_only") << 5 | int(script == "request_only") << 6)
        controls.append([path, old_codes[m25], old_codes[m26], bits, new_codes[category]])
    return {key: result[key] for key in ("read_only", "source", "physical_controls",
                                        "class_counts", "input_type_counts", "subtree_counts")} | {
        "control_record_schema": ["release_unique_path", "m25_category_code",
                                  "m26_category_code", "capability_bits", "m27_class_code"],
        "capability_bit_schema": {"render": 1, "pointer": 2, "focus": 4,
                                  "keyboard": 8, "action_queue": 16,
                                  "audio_request_only": 32, "script_request_only": 64},
        "legacy_category_codes": old_codes, "m27_class_codes": new_codes,
        "controls": controls}


if __name__ == "__main__":
    print(json.dumps(compact(inventory(Path(sys.argv[1]), Path(sys.argv[2]))),
                     ensure_ascii=False, separators=(",", ":")))
