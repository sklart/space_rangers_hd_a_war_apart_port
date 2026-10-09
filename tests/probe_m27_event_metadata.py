#!/usr/bin/env python3
"""Read-only release inventory of UI script/event metadata occurrences."""
from collections import Counter, defaultdict
from pathlib import Path
import json
import sys

import probe_m23_text_release as config
from probe_ui_images_release import decode_dat


FIELDS = ("OnPressCode", "OnMouseEnterCode", "OnMouseLeaveCode",
          "OnMouseRightClick", "OnKey")


def oracle(root: Path) -> dict:
    all_nodes = list(config.nodes(config.parse_blocks(decode_dat(root / "CFG/Main.dat"))))
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in all_nodes if node.path.startswith("ML/Style/")}
    raw = Counter()
    on_blocks = Counter(node.name for node in all_nodes if node.name.startswith("On"))
    physical = Counter()
    examples: dict[str, list[str]] = defaultdict(list)
    for node in all_nodes:
        if not node.path.startswith("ML/"):
            continue
        for key, value in node.properties:
            if key in FIELDS and value.strip():
                raw[key] += 1
        if node.path.startswith("ML/Style/"):
            continue
        props = config.effective(config.style_chain(node, styles))
        for key in FIELDS:
            if any(value.strip() for value in props.get(key, [])):
                physical[key] += 1
                if len(examples[key]) < 5:
                    examples[key].append(node.unique_path)
    return {"source": "CFG/Main.dat", "parsed_nodes": len(all_nodes),
            "requested_block_occurrences": {key: on_blocks[key] for key in FIELDS},
            "other_on_blocks": {key: value for key, value in on_blocks.items() if key not in FIELDS},
            "requested_property_occurrences": {key: raw[key] for key in FIELDS},
            "physical_effective_property_nodes": {key: physical[key] for key in FIELDS},
            "examples": dict(examples)}


if __name__ == "__main__":
    print(json.dumps(oracle(Path(sys.argv[1])), ensure_ascii=False, indent=2))
