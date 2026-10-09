#!/usr/bin/env python3
"""Read-only source/decoded oracle for M26's programmatic real-GI GraphBuf."""
from __future__ import annotations

import argparse
import json
import mmap
from pathlib import Path

import probe_gai_release as gi
from probe_ui_images_release import entries, payload


RESOURCE = "DATA/FormLoad2/2BarCenter.gi"


def oracle(root: Path) -> dict:
    with (root / "DATA/forms.pkg").open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
        item = next(item for item in entries(mapped) if item[0].casefold() == RESOURCE.casefold())
        source = payload(mapped, item)
    meta, pixels = gi.decode2(source)
    width = meta["decoded"]["width"]
    height = meta["decoded"]["height"]
    assert (width, height) == (29, 37)
    fit = (round(50 * width / height), 50)
    assert fit == (39, 50)
    return {"resource": RESOURCE, "source": gi.fp(source),
            "decoded_size": (width, height), "decoded_bgra": gi.fp(pixels),
            "target_size": (100, 50), "scaled_size": fit,
            "filter": "OKGF_RESCALE_LANCZOS3"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    args = parser.parse_args()
    print(json.dumps(oracle(args.game_root), indent=2))
