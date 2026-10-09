#!/usr/bin/env python3
"""Read-only physical Edit inventory from licensed release configuration."""
from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path

import probe_m23_text_release as m23
from probe_ui_images_release import decode_dat


BASE = frozenset(("Style", "Pos", "PosZ", "Size", "Sme", "Name", "Active",
                  "Help", "MVUpdate", "MouseBlocking", "MouseBlockingTest"))
EDIT = frozenset(("Font", "ReturnFocusLeave", "Text", "Image", "TextColor",
                  "Border", "BorderLightColor", "BorderDarkColor", "CursorColor",
                  "MaxLen", "AlignX"))


def inventory(game_root: Path, language: str) -> dict:
    nodes = list(m23.nodes(m23.parse_blocks(decode_dat(game_root / "CFG/Main.dat"))))
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in nodes if node.path.startswith("ML/Style/")}
    physical = [node for node in nodes if node.name == "Edit"
                and not node.path.startswith("ML/Style/")]
    font_keys = {m23.effective(m23.style_chain(node, styles)).get("Font", [""])[-1]
                 for node in physical}
    fonts = m23.font_sources(game_root, font_keys - {""})
    translations = m23.language_map(game_root / "CFG" / language / "Lang.dat")
    records = []
    for node in physical:
        effective = m23.effective(m23.style_chain(node, styles))
        props = {key: values[-1] for key, values in effective.items()}
        font_key = props.get("Font", "")
        raw_text = props.get("Text", "")
        resolved_text = "\n".join(translations[raw_text]) if raw_text in translations else raw_text
        image = props.get("Image", "")
        align = props.get("AlignX", "Left")
        unknown = sorted(set(props) - BASE - EDIT)
        try:
            max_length = int(props.get("MaxLen", "256"))
        except ValueError:
            max_length = -1
        render_eligible = bool(fonts.get(font_key)) and align in ("Left", "Center") and \
            max_length >= 0 and not unknown and not image
        records.append({"path": node.unique_path, "style": props.get("Style", ""),
                        "font_key": font_key, "font_resolved": bool(fonts.get(font_key)),
                        "font_source": {k: v for k, v in fonts.get(font_key, {}).items()
                                        if k != "codes"},
                        "size": props.get("Size", ""), "text_key": raw_text,
                        "text": resolved_text, "localized": raw_text in translations,
                        "image_key": image, "border": props.get("Border", "False"),
                        "text_color": props.get("TextColor", "255,255,255"),
                        "cursor_color": props.get("CursorColor", "255,0,0"),
                        "max_length": max_length, "align_x": align,
                        "return_focus_leave": props.get("ReturnFocusLeave", "True"),
                        "unknown_properties": unknown,
                        "render_eligible": render_eligible,
                        "interactive_eligible": False,
                        "category": "RENDER_ONLY" if render_eligible else "UNSUPPORTED"})
    eligible = [record for record in records if record["render_eligible"]]
    return {"read_only": True, "physical": len(records),
            "styles": dict(sorted(Counter(item["style"] for item in records).items())),
            "fonts": dict(sorted(Counter(item["font_key"] for item in records).items())),
            "alignments": dict(sorted(Counter(item["align_x"] for item in records).items())),
            "max_lengths": dict(sorted(Counter(item["max_length"] for item in records).items())),
            "with_text": sum(bool(item["text"]) for item in records),
            "with_image": sum(bool(item["image_key"]) for item in records),
            "with_border": sum(item["border"].casefold() == "true" for item in records),
            "render_eligible": len(eligible),
            "deterministic_candidate": min(eligible, key=lambda item: item["path"].casefold())
                                       if eligible else None,
            "controls": records}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    parser.add_argument("--language", default="Rus")
    args = parser.parse_args()
    print(json.dumps(inventory(args.game_root, args.language), ensure_ascii=False, indent=2))
