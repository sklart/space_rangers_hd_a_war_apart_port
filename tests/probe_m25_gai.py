#!/usr/bin/env python3
"""Independent release GAI sequence, frame, origin and timing oracle."""
from __future__ import annotations

import argparse
import json
import mmap
from pathlib import Path
import struct

import probe_gai_release as oracle
from probe_ui_images_release import entries, payload

RESOURCE = "DATA/PI/PathEndMove.gai"


def inspect(root: Path) -> dict:
    with (root / "DATA/common.pkg").open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
        entry = next(e for e in entries(mapped) if e[0].casefold() == RESOURCE.casefold())
        source = payload(mapped, entry)
    metadata = oracle.gai(source)
    assert metadata["flags"] == 0 and metadata["sequence_count"] >= 1
    sequence = metadata["sequences"][0]
    rows = []
    for sequence_frame in (0, 1):
        index = sequence["source_indices"][sequence_frame]
        _, gi, _, _ = oracle.frame(source, metadata, index)
        frame_meta, pixels = oracle.decode2(gi)
        canvas = [0] * (32 * 32)
        ox = frame_meta["bounds"][0] - metadata["bounds"][0]
        oy = frame_meta["bounds"][1] - metadata["bounds"][1]
        for y in range(frame_meta["decoded"]["height"]):
            for x in range(frame_meta["decoded"]["width"]):
                at = (y * frame_meta["decoded"]["width"] + x) * 4
                blue, green, red, alpha = pixels[at:at + 4]
                if not alpha: continue
                target = (y + oy) * 32 + x + ox
                old = canvas[target]
                dr, dg, db = ((old >> 11 & 31) * 255 // 31,
                              (old >> 5 & 63) * 255 // 63, (old & 31) * 255 // 31)
                blend = lambda src, dst: (src * alpha + dst * (255 - alpha) + 127) // 255
                r, g, b = blend(red, dr), blend(green, dg), blend(blue, db)
                canvas[target] = (r >> 3 << 11) | (g >> 2 << 5) | (b >> 3)
        rows.append({"sequence_frame": sequence_frame, "source_frame": index,
                     "delay_ms": sequence["frame_delays"][sequence_frame],
                     "bounds": frame_meta["bounds"],
                     "offset": [frame_meta["bounds"][0] - metadata["bounds"][0],
                                frame_meta["bounds"][1] - metadata["bounds"][1]],
                     "pixels": oracle.fp(pixels),
                     "canvas_rgb565": oracle.fp(b"".join(struct.pack("<H", value) for value in canvas))})
    return {"resource": RESOURCE, "source": oracle.fp(source),
            "canvas_bounds": metadata["bounds"], "frame_count": metadata["frame_count"],
            "sequence_count": metadata["sequence_count"],
            "sequence_length": sequence["frame_count"],
            "first_two_frames": rows}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.game_root), ensure_ascii=False, indent=2))
