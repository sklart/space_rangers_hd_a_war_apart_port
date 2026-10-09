#!/usr/bin/env python3
"""Independent release-property and upstream-order oracle for M11Clear events."""
import json
from pathlib import Path
import struct
import sys
import zlib

import probe_m23_text_release as config
from probe_ui_images_release import decode_dat


def oracle(root: Path) -> dict:
    nodes = list(config.nodes(config.parse_blocks(decode_dat(root / "CFG/Main.dat"))))
    styles = {"Style." + n.path.removeprefix("ML/Style/").replace("/", "."): n
              for n in nodes if n.path.startswith("ML/Style/")}
    panel = next(n for n in nodes if n.unique_path ==
                 "ML#0/Info#0/Panel#0/Panel#4/Panel#11")
    button = next(n for n in panel.children if n.last("Name") == "M11Clear")
    properties = config.effective(config.style_chain(button, styles))
    expected = {
        "Kind": ["Normal"], "KindHit": ["Rect"],
        "Pos": ["128,407,1"], "Size": ["67,21"],
        "SoundEnter": ["Sound.ButtonEnter"],
        "SoundClick": ["Sound.ButtonClick"],
        "SoundLeave": ["Sound.ButtonLeave"],
        "ImageNormalA": ["GI,Bm.FormInfo2.2FindA"],
        "ImageDown": ["GI,Bm.FormInfo2.2FindD"],
    }
    for key, value in expected.items():
        assert properties.get(key) == value, (key, properties.get(key))

    # The release panel places an ordinary image below this button. The
    # upstream recursive dispatcher visits active hit children in tree order;
    # the button's two image children also receive enter/leave and its first
    # image receives the press. These paths describe that release topology.
    base = "root/PanelM11"
    image = base + "/2#0"
    button_path = base + "/M11Clear"
    normal = button_path + "/2#1"
    caption = button_path + "/4#0"
    enter = ["root", base, image, button_path, normal, caption]
    leave = ["root", base, image, button_path, caption]
    # Event kind values are the portable wire format, independent of the C++
    # enum declaration: enter=0, leave=1, hover-gained=2, hover-lost=3,
    # button-down=6, button-up=7, activate=8, sound-requested=17.
    actions = [(0, p, 0, "", 0, 0, 0) for p in enter[:4]]
    actions.extend([
        (17, button_path, 0, properties["SoundEnter"][0], 0, 0, 0),
        (2, button_path, 0, "", 0, 0, 1),
        (0, normal, 0, "", 0, 0, 0),
        (0, caption, 0, "", 0, 0, 0),
        (1, normal, 0, "", 0, 0, 0),
        (6, button_path, 1, "", 0, 0, 1),
        (17, button_path, 0, properties["SoundClick"][0], 0, 0, 0),
        (7, button_path, 0, "", 0, 1, 0),
        (8, button_path, 0, "", 0, 0, 0),
    ])
    actions.extend((1, p, 0, "", 0, 0, 0) for p in leave)
    actions.extend([
        (17, button_path, 0, properties["SoundLeave"][0], 0, 0, 0),
        (3, button_path, 0, "", 0, 1, 0),
    ])
    data = bytearray(struct.pack("<I", len(actions)))
    for sequence, (kind, path, value, payload, detail, old, new) in enumerate(actions, 1):
        data += struct.pack("<QI", sequence, kind)
        for encoded in (path.encode(),):
            data += struct.pack("<I", len(encoded)) + encoded
        data += struct.pack("<iii", 459, 537, value)
        encoded = payload.encode()
        data += struct.pack("<I", len(encoded)) + encoded
        data += struct.pack("<iii", detail, old, new)
    fnv = 0xcbf29ce484222325
    for byte in data:
        fnv = ((fnv ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return {"candidate": button.unique_path, "point": [459, 537],
            "action_count": len(actions), "byte_count": len(data),
            "crc32": f"{zlib.crc32(data):08x}", "fnv64": f"{fnv:016x}"}


if __name__ == "__main__":
    print(json.dumps(oracle(Path(sys.argv[1])), indent=2))
