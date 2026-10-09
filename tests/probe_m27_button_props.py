#!/usr/bin/env python3
"""Inspect release properties of the deterministic M27 Info button."""
from pathlib import Path
import json
import sys

import probe_m23_text_release as config
from probe_ui_images_release import decode_dat


def main(root: Path) -> None:
    all_nodes = list(config.nodes(config.parse_blocks(decode_dat(root / "CFG/Main.dat"))))
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in all_nodes if node.path.startswith("ML/Style/")}
    panel = next(node for node in all_nodes if node.unique_path ==
                 "ML#0/Info#0/Panel#0/Panel#4/Panel#11")
    button = next(child for child in panel.children if child.last("Name") == "M11Clear")
    props = config.effective(config.style_chain(button, styles))
    print(json.dumps({key: value for key, value in props.items()
                      if key.startswith("Image") or key.startswith("Caption") or
                      key.startswith("Sound") or
                      key in ("Down", "Kind", "KindHit", "Pos", "Size")},
                     ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main(Path(sys.argv[1]))
