#!/usr/bin/env python3
"""Read-only M28 preparation: unresolved Image roots and runtime GraphBuf owners."""
from collections import Counter
from pathlib import Path
import json
import sys

import probe_m23_text_release as m23
import probe_m24_controls_release as m24
import probe_m26_eligibility as m26
from probe_ui_images_release import KNOWN_CONTROLS, decode_dat


def read(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def code_evidence(source_root: Path, screen: str, name: str):
    if not name:
        return []
    aliases = {"AB": ["ab_MainForm.cpp"], "ModsManager": ["fMods.cpp"]}
    files = sorted(source_root.glob("f" + screen + "*.cpp"))
    files.extend(source_root / alias for alias in aliases.get(screen, []))
    evidence = []
    for file in files:
        lines = file.read_text(encoding="utf-8", errors="replace").splitlines()
        for index, line in enumerate(lines):
            if '"' + name + '"' not in line:
                continue
            neighborhood = "\n".join(lines[max(0, index - 8):index + 12])
            operations = [op for op in ("BindExternalGraphBuf", "AllocateBuffer",
                        "CopyScreenRectToBuffer") if op in neighborhood]
            evidence.append({"file": file.name, "line": index + 1,
                             "operations_nearby": operations})
    return evidence[:5]


def audit(game_root: Path, project: Path):
    parsed = m23.parse_blocks(decode_dat(game_root / "CFG/Main.dat"))
    all_nodes = list(m23.nodes(parsed))
    by_path = {node.unique_path: node for node in all_nodes}
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in all_nodes if node.path.startswith("ML/Style/")}
    nodes = [node for node in all_nodes if node.name in KNOWN_CONTROLS
             and not node.path.startswith("ML/Style/")]
    prior = {row["path"]: row for row in read(project / "m25-eligibility.json")["after_gai"]["controls"]}
    added = {row["path"]: row for kind, filename in (
        ("GraphBuf", "m26-graphbuf-inventory.json"),
        ("ScrollBar", "m26-scrollbar-inventory.json"),
        ("PanelScrollBar", "m26-panel-scrollbar-inventory.json"),
        ("Edit", "m26-edit-inventory.json"))
        for row in read(project / filename)["controls"]}
    def eligible(node):
        row = added.get(node.unique_path) or prior.get(node.unique_path)
        return bool(row and (row.get("render_eligible") or row.get("category") == "RENDER_ONLY"))

    cache, available = m24.image_sources(game_root)
    image_roots = []
    for panel in (node for node in nodes if node.name in ("Panel", "Window", "PanelScrollBar")):
        descendants = [panel, *(node for node in m23.nodes(panel) if node.name in KNOWN_CONTROLS)]
        depth = max((node.unique_path.count("/") - panel.unique_path.count("/") + 1
                     for node in descendants), default=0)
        visual = sum(node.name in m26.VISUAL for node in descendants)
        if depth < 2 or visual < 3 or "Image" not in {n.name for n in descendants if not eligible(n)}:
            continue
        problems = []
        for image in (n for n in descendants if n.name == "Image" and not eligible(n)):
            mode, key = m23.image_reference(image, styles)
            if not key:
                reason = "empty_image_key"
            elif not m24.resource_resolved(key, cache, available):
                reason = "runtime_dependent_or_unresolved_cache_key"
            elif mode == "GI":
                reason = "unsupported_GI_format_or_render_semantics"
            elif mode in ("Anim", "AImage"):
                reason = "conditional_animation_state"
            else:
                reason = "other_render_semantics"
            problems.append({"path": image.unique_path, "name": image.last("Name") or "",
                             "mode": mode, "key": key, "reason": reason})
        image_roots.append({"path": panel.unique_path, "images": problems})
    assert len(image_roots) == 104, len(image_roots)

    source_root = project / "upstream/cpp/src"
    graphbuf = []
    for row in read(project / "m26-graphbuf-inventory.json")["controls"]:
        path = row["path"]
        name = by_path[path].last("Name") or ""
        screen = path.split("/")[1].split("#")[0]
        evidence = code_evidence(source_root, screen, name)
        operations = sorted({op for item in evidence for op in item["operations_nearby"]})
        graphbuf.append({"path": path, "name": name, "screen": screen,
                         "source": "RUNTIME_DATA_REQUIRED", "producer_operations": operations,
                         "direct_code_references": evidence,
                         "producer_class": (operations[0] if operations else
                             "runtime_drawing_producer" if evidence else "other_unresolved"),
                         "producer_status": "direct_reference_found" if evidence else "static_owner_unresolved"})
    assert len(graphbuf) == 68
    return {"read_only": True, "source": "release Main.dat plus upstream C++ references",
            "image_blocker_roots": len(image_roots),
            "image_reason_counts_overlapping": dict(Counter(p["reason"] for r in image_roots for p in r["images"])),
            "image_roots": image_roots,
            "dynamic_graphbuf_controls": len(graphbuf),
            "graphbuf_producer_status_counts": dict(Counter(r["producer_status"] for r in graphbuf)),
            "graphbuf": graphbuf}


def compact(result):
    return {"read_only": result["read_only"], "source": result["source"],
            "image_blocker_roots": result["image_blocker_roots"],
            "image_reason_counts_overlapping": result["image_reason_counts_overlapping"],
            "image_roots": [{"path": root["path"], "image_count": len(root["images"]),
                             "reasons": dict(Counter(image["reason"] for image in root["images"]))}
                            for root in result["image_roots"]],
            "dynamic_graphbuf_controls": result["dynamic_graphbuf_controls"],
            "graphbuf_producer_status_counts": result["graphbuf_producer_status_counts"],
            "graphbuf": [{"path": item["path"], "name": item["name"],
                          "source": item["source"], "producer_class": item["producer_class"],
                          "evidence": [f'{ref["file"]}:{ref["line"]}'
                                       for ref in item["direct_code_references"]]}
                         for item in result["graphbuf"]]}


if __name__ == "__main__":
    print(json.dumps(compact(audit(Path(sys.argv[1]), Path(sys.argv[2]))),
                     ensure_ascii=False, indent=2))
