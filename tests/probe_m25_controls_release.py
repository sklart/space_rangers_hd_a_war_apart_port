#!/usr/bin/env python3
"""Independent real GraphButton and Window GI layout/RGB565 oracle."""
from __future__ import annotations

import argparse
import json
import mmap
from pathlib import Path
import struct

import probe_gai_release as gi
import probe_m23_text_release as config
from probe_ui_images_release import decode_dat, entries, payload

BUTTON_PATH = "ML#0/AB#0/Panel#0/Panel#0/Panel#1/Panel#1/GraphButton#0"
WINDOW_PATH = "ML#0/AB#0/Panel#0/Panel#0/Panel#1/Window#0"
BUTTON_STATES = ("ImageNormal", "ImageNormalA", "ImageDown", "ImageDisable")
WINDOW_ORDER = ("ImageLeft", "ImageRight", "ImageTop", "ImageBottom",
                "ImageTopLeft", "ImageTopRight", "ImageBottomLeft",
                "ImageBottomRight", "ImageTexture")


def draw(framebuffer: list[int], width: int, height: int, image: tuple[dict, bytes],
         x: int, y: int, clip: tuple[int, int, int, int]) -> None:
    meta, pixels = image
    iw, ih = meta["decoded"]["width"], meta["decoded"]["height"]
    for sy in range(ih):
        dy = y + sy
        if not clip[1] <= dy < clip[3] or not 0 <= dy < height: continue
        for sx in range(iw):
            dx = x + sx
            if not clip[0] <= dx < clip[2] or not 0 <= dx < width: continue
            blue, green, red, alpha = pixels[(sy * iw + sx) * 4:(sy * iw + sx) * 4 + 4]
            if alpha == 0: continue
            at = dy * width + dx
            old = framebuffer[at]
            dr, dg, db = ((old >> 11 & 31) * 255 // 31,
                          (old >> 5 & 63) * 255 // 63, (old & 31) * 255 // 31)
            blend = lambda src, dst: (src * alpha + dst * (255 - alpha) + 127) // 255
            r, g, b = blend(red, dr), blend(green, dg), blend(blue, db)
            framebuffer[at] = (r >> 3 << 11) | (g >> 2 << 5) | (b >> 3)


def image_data(root: Path, keys: set[str]) -> dict[str, dict]:
    cache = {path.replace("/", ".").casefold(): name.replace("\\", "/").casefold()
             for path, name in config.data_entries(decode_dat(root / "CFG/CacheData.dat", 0xEA8F3F37))}
    out = {}
    with (root / "DATA/forms.pkg").open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
        index = {e[0].replace("\\", "/").casefold(): e for e in entries(mapped)}
        for key in keys:
            entry = index[cache[key.casefold()]]
            source = payload(mapped, entry)
            meta, pixels = gi.decode2(source)
            out[key] = {"path": entry[0], "source": gi.fp(source),
                        "bounds": meta["bounds"], "size": meta["decoded"],
                        "image": (meta, pixels)}
    return out


def render(root: Path) -> dict:
    tree = config.parse_blocks(decode_dat(root / "CFG/Main.dat"))
    nodes = list(config.nodes(tree))
    styles = {"Style." + n.path.removeprefix("ML/Style/").replace("/", "."): n
              for n in nodes if n.path.startswith("ML/Style/")}
    button = next(n for n in nodes if n.unique_path == BUTTON_PATH)
    window = next(n for n in nodes if n.unique_path == WINDOW_PATH)
    bp, wp = (config.effective(config.style_chain(n, styles)) for n in (button, window))
    assert bp["Kind"][-1] == "Disable" and bp["KindHit"][-1] == "Graph"
    assert wp["Active"][-1] == "False"  # activated only for the border oracle
    all_keys = {bp[slot][-1].split(",", 1)[1] for slot in BUTTON_STATES}
    all_keys.update(wp[slot][-1].split(",", 1)[1] for slot in WINDOW_ORDER)
    assets = image_data(root, all_keys)
    button_frames = {}
    for state, slot in (("normal", "ImageNormal"), ("hover", "ImageNormalA"),
                        ("down", "ImageDown")):
        key = bp[slot][-1].split(",", 1)[1]
        framebuffer = [0] * (53 * 42)
        draw(framebuffer, 53, 42, assets[key]["image"], 0, 0, (0, 0, 53, 42))
        button_frames[state] = gi.fp(b"".join(struct.pack("<H", v) for v in framebuffer))
    sizes = {slot: (assets[wp[slot][-1].split(",", 1)[1]]["size"]["width"],
                    assets[wp[slot][-1].split(",", 1)[1]]["size"]["height"])
             for slot in WINDOW_ORDER}
    requested = tuple(map(int, wp["Size"][-1].split(",")))
    minimum = tuple(map(int, wp["MinSize"][-1].split(",")))
    base_width = sizes["ImageTopLeft"][0] + sizes["ImageTopRight"][0]
    base_height = sizes["ImageTopLeft"][1] + sizes["ImageBottomLeft"][1]
    width = max(requested[0], minimum[0])
    height = max(requested[1], minimum[1])
    width = base_width + max(0, (width - base_width + sizes["ImageTop"][0] - 1) //
                            sizes["ImageTop"][0] * sizes["ImageTop"][0])
    height = base_height + max(0, (height - base_height + sizes["ImageLeft"][1] - 1) //
                              sizes["ImageLeft"][1] * sizes["ImageLeft"][1])
    tl, tr = sizes["ImageTopLeft"], sizes["ImageTopRight"]
    bl, br = sizes["ImageBottomLeft"], sizes["ImageBottomRight"]
    top, bottom = sizes["ImageTop"], sizes["ImageBottom"]
    left, right = sizes["ImageLeft"], sizes["ImageRight"]
    placements = {
        "ImageTopLeft": (0, 0, *tl, False, False),
        "ImageTopRight": (width-tr[0], 0, *tr, False, False),
        "ImageBottomLeft": (0, height-bl[1], *bl, False, False),
        "ImageBottomRight": (width-br[0], height-br[1], *br, False, False),
        "ImageTop": (tl[0], 0, width-tl[0]-tr[0], top[1], True, False),
        "ImageBottom": (bl[0], height-bottom[1], width-bl[0]-br[0], bottom[1], True, False),
        "ImageLeft": (0, tl[1], left[0], height-tl[1]-bl[1], False, True),
        "ImageRight": (width-right[0], tr[1], right[0], height-tr[1]-br[1], False, True),
        "ImageTexture": (left[0], top[1], width-left[0]-right[0],
                         height-top[1]-bottom[1], True, True),
    }
    frame = [0] * (width * height)
    # Equal-depth border attachments are stored in reverse insertion order.
    for slot in reversed(WINDOW_ORDER):
        key = wp[slot][-1].split(",", 1)[1]
        x, y, w, h, fill_x, fill_y = placements[slot]
        iw, ih = sizes[slot]
        clip = (x, y, x+w, y+h)
        for ty in range(y, y+h, ih) if fill_y else (y+(h-ih)//2,):
            for tx in range(x, x+w, iw) if fill_x else (x+(w-iw)//2,):
                draw(frame, width, height, assets[key]["image"], tx, ty, clip)
    return {"button": {"path": BUTTON_PATH, "kind": bp["Kind"][-1],
                       "kind_hit": bp["KindHit"][-1], "size": [53, 42],
                       "resources": {slot: bp[slot][-1] for slot in BUTTON_STATES},
                       "frames": button_frames},
            "window": {"path": WINDOW_PATH, "resources": {slot: wp[slot][-1] for slot in WINDOW_ORDER},
                       "natural_sizes": sizes, "requested_size": requested,
                       "aligned_size": [width, height],
                       "work_sub_rect": wp["WorkSubRect"][-1],
                       "placements": placements,
                       "frame": gi.fp(b"".join(struct.pack("<H", v) for v in frame))},
            "sources": {key: {field: asset[field] for field in ("path", "source", "bounds")}
                        for key, asset in sorted(assets.items())}}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    args = parser.parse_args()
    print(json.dumps(render(args.game_root), ensure_ascii=False, indent=2))
