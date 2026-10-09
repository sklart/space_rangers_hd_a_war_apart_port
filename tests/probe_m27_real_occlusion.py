#!/usr/bin/env python3
"""Release metadata for the bounded M27 real MouseBlocking geometry test."""
from pathlib import Path
import json
import sys

import probe_m23_text_release as m23
from probe_ui_images_release import decode_dat


def oracle(root: Path):
    nodes = list(m23.nodes(m23.parse_blocks(decode_dat(root / "CFG/Main.dat"))))
    styles = {"Style." + n.path.removeprefix("ML/Style/").replace("/", "."): n
              for n in nodes if n.path.startswith("ML/Style/")}
    path = "ML#0/Info#0/Panel#0/Panel#0/Panel#0"
    node = next(n for n in nodes if n.unique_path == path)
    props = m23.effective(m23.style_chain(node, styles))
    assert node.last("Name") == "PM_PanelMsg"
    assert props.get("MouseBlocking") == ["True"]
    assert props.get("Pos") == ["111,741,2"]
    assert props.get("Size") == ["362,26"]
    return {"path": path, "name": "PM_PanelMsg", "mouse_blocking": True,
            "configured_position": [111, 741], "configured_size": [362, 26],
            "test_point": [200, 750], "scope": "config geometry and portable query only"}


if __name__ == "__main__":
    print(json.dumps(oracle(Path(sys.argv[1])), indent=2))
