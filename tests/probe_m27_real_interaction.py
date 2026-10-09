#!/usr/bin/env python3
"""Independent release-resource RGB565 oracle for the M27 M11Clear sequence."""
from pathlib import Path
import json
import sys

import probe_m26_real_showcase as showcase


def oracle(root: Path) -> dict:
    normal = showcase.oracle(root, button_state="normal")["frame"]
    hover = showcase.oracle(root, button_state="hover")["frame"]
    down = showcase.oracle(root, button_state="down")["frame"]
    # M26 release inventory: Min=1, Max=200, up/down=21, thumb=56,
    # client length=251. CalculationMode=0 uses a 153-pixel drag denominator.
    minimum, maximum, position = 1, 200, 1
    denominator = 251 - 21 - 21 - 56
    drag_start = position - (maximum - minimum) * (50 - 21) // denominator
    dragged = max(minimum, min(maximum,
        drag_start + (maximum - minimum) * (90 - 21) // denominator))
    after_press = dragged - 1
    after_delay = after_press - 1
    after_repeats = after_delay - 3
    return {"path": showcase.PATH, "button": "M11Clear", "hit": "Rect",
            "point": [459, 537], "frames": {
                "normal": normal, "hover": hover, "down": down,
                "up": hover, "leave": normal},
            "scroll": {"path": "ML#0/Film#0/Panel#0/Panel#0/Panel#0/ScrollBar#0",
                       "drag_denominator": denominator, "position_before": position,
                       "position_after_drag": dragged,
                       "position_after_press": after_press,
                       "position_after_500ms": after_delay,
                       "position_after_150ms_more": after_repeats}}


if __name__ == "__main__":
    print(json.dumps(oracle(Path(sys.argv[1])), ensure_ascii=False, indent=2))
