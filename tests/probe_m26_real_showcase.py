#!/usr/bin/env python3
"""Independent RGB565 oracle for the 50-node Info/PanelM11 release fragment."""
from __future__ import annotations

import argparse
import json
import mmap
from pathlib import Path
import struct

import probe_aft_release as aft
import probe_gai_release as gi
import probe_m23_real_label as label_oracle
import probe_m23_text_release as config
import probe_m25_controls_release as image_oracle
from probe_ui_images_release import data_entries, decode_dat, entries, payload


PATH = "ML#0/Info#0/Panel#0/Panel#4/Panel#11"
WIDTH, HEIGHT = 1280, 720


def dimensions(props: dict[str, list[str]]) -> tuple[int, int, int, int, int]:
    x, y, *z = (int(part) for part in props.get("Pos", ["0,0,0"])[-1].split(","))
    w, h = (int(part) for part in props["Size"][-1].split(","))
    return x, y, w, h, z[0] if z else 0


def wrapped(text: str, advances: dict, width: int) -> list[str]:
    # Release line has only ordinary glyphs; no tagged spans or embedded objects.
    result = []
    at = 0
    while at < len(text):
        end, best, current = at, at, 0
        while end < len(text):
            following = current + advances.get(text[end], (0, []))[0]
            if following > width and end > at:
                break
            current = following
            if text[end] == " ": best = end + 1
            end += 1
            if following > width: break
        if end == at: end += 1
        if end < len(text) and best > at: end = best
        line = text[at:end]
        if result: line = line.lstrip(" ")
        result.append(line)
        at = end
    return result


def measure(text: str, glyphs: dict) -> tuple[int, int, int, int]:
    pen = 0
    left, top, right, bottom = 2**31 - 1, 2**31 - 1, -(2**31), -(2**31)
    for char in text:
        advance, planes = glyphs[char]
        for gx, gy, gw, gh, _ in planes:
            left, top = min(left, pen + gx), min(top, gy)
            right, bottom = max(right, pen + gx + gw), max(bottom, gy + gh)
        pen += advance
    if left == 2**31 - 1: left, top, bottom = min(0, pen), 0, 0
    return left, top, max(right, pen), bottom


def render_text(frame: list[int], text: str, font: dict, rect: tuple[int, int, int, int],
                align_x: str, align_y: str, color: int, wrap: bool = False) -> dict:
    x, y, w, h = rect
    glyphs, metrics = font["glyphs"], font["metrics"]
    lines = wrapped(text, glyphs, w - 4) if wrap else [text]
    bounds = [measure(line, glyphs) for line in lines]
    first = bounds[0]
    left = min(bound[0] for bound in bounds)
    right = max(bound[2] for bound in bounds)
    top = min(bound[1] + index * metrics["line_height"] for index, bound in enumerate(bounds))
    bottom = max(bound[3] + index * metrics["line_height"] for index, bound in enumerate(bounds))
    content_height = bottom - top + 2
    if wrap or align_x == "Left": text_left = x + 2
    elif align_x == "Right": text_left = x + w - (right - left) - 2
    else: text_left = x + w // 2 - (right - left) // 2
    if align_y == "Top": text_top = y + 2
    elif align_y == "Bottom": text_top = y + h - content_height - 2
    else: text_top = y + h // 2 - content_height // 2
    baseline = text_top + metrics["above_baseline"] - 2
    if align_y == "CenterEx" and not wrap:
        baseline = y + h // 2 - (metrics["line_height"] * (len(lines) - 1) +
                                 metrics["centering_height"]) // 2 + metrics["centering_height"]
    drawn = 0
    for line_number, line in enumerate(lines):
        bound = bounds[line_number]
        start_x = text_left
        if not wrap and align_x == "Right": start_x = x + w - (bound[2] - bound[0]) - 2
        elif not wrap and align_x == "Center": start_x = x + w // 2 - (bound[2] - bound[0]) // 2
        pen = start_x
        for char in line:
            advance, planes = glyphs[char]
            for gx, gy, gw, gh, encoded in planes:
                for sx, sy in label_oracle.mask_pixels(encoded, gw, gh):
                    px, py = pen + gx + sx, baseline + line_number * metrics["line_height"] + gy + sy
                    if x <= px < x + w and y <= py < y + h and 0 <= px < WIDTH and 0 <= py < HEIGHT:
                        frame[py * WIDTH + px] = color
                        drawn += 1
            pen += advance
    return {"lines": len(lines), "pixels": drawn, "baseline": baseline}


def oracle(root: Path, frame_output: Path | None = None) -> dict:
    all_nodes = list(config.nodes(config.parse_blocks(decode_dat(root / "CFG/Main.dat"))))
    node = next(item for item in all_nodes if item.unique_path == PATH)
    assert node.name == "Panel" and node.last("Name") == "PanelM11" and len(node.children) == 49
    assert node.last("Active") == "False"  # The hardware showcase explicitly opens this tab.
    styles = {"Style." + item.path.removeprefix("ML/Style/").replace("/", "."): item
              for item in all_nodes if item.path.startswith("ML/Style/")}
    translations = config.language_map(root / "CFG/Rus/Lang.dat")
    cache = {name.replace("/", ".").casefold(): path.replace("\\", "/").casefold()
             for name, path in data_entries(decode_dat(root / "CFG/CacheData.dat", 0xEA8F3F37))}
    selected = []
    keys = set()
    font_keys = set()
    for index, child in enumerate(node.children):
        props = config.effective(config.style_chain(child, styles))
        x, y, w, h, depth = dimensions(props)
        selected.append((index, child, props, (298 + x, 120 + y, w, h), depth))
        if child.name == "Image": keys.add(props["Image"][-1].split(",", 1)[1])
        if child.name == "GraphButton":
            slot = "ImageDown" if props.get("Down", ["False"])[-1] == "True" else "ImageNormal"
            keys.add(props[slot][-1].split(",", 1)[1])
        if child.name in ("Label", "Edit") or props.get("Caption"):
            font_keys.add(props.get("Font", [""])[-1])
    assert font_keys == {"Font.2Normal", "Font.2Ranger", "Font.2Small", "Font.2SmallBold"}
    fonts = {}
    for key in font_keys:
        _, resource, source = label_oracle.resolve_font(root, key)
        fonts[key] = {"resource": resource, "source": aft.fingerprint(source),
                      "glyphs": label_oracle.glyphs(source), "metrics": aft.aft(source)}
    assets = {}
    with (root / "DATA/forms.pkg").open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
        index = {item[0].replace("\\", "/").casefold(): item for item in entries(mapped)}
        for key in keys:
            item = index[cache[key.casefold()]]
            source = payload(mapped, item)
            meta, pixels = gi.decode2(source)
            assets[key] = {"path": item[0], "source": gi.fp(source), "image": (meta, pixels)}
    frame = [0] * (WIDTH * HEIGHT)
    drawn = []
    # UiObject inserts before equal-depth siblings; higher depths draw first.
    for _, child, props, rect, depth in sorted(selected, key=lambda row: (-row[4], -row[0])):
        x, y, w, h = rect
        clip = (max(298, x), max(120, y), min(708, x + w), min(555, y + h))
        if props.get("Active", ["True"])[-1] == "False":
            continue
        if child.name == "Image":
            key = props["Image"][-1].split(",", 1)[1]
            meta = assets[key]["image"][0]["decoded"]
            assert (w, h) == (meta["width"], meta["height"])
            image_oracle.draw(frame, WIDTH, HEIGHT, assets[key]["image"], x, y, clip)
            drawn.append((child.name, key))
        elif child.name == "GraphButton":
            slot = "ImageDown" if props.get("Down", ["False"])[-1] == "True" else "ImageNormal"
            key = props[slot][-1].split(",", 1)[1]
            meta = assets[key]["image"][0]["decoded"]
            assert (w, h) == (meta["width"], meta["height"]), (child.last("Name"), (w, h), meta)
            image_oracle.draw(frame, WIDTH, HEIGHT, assets[key]["image"], x, y, clip)
            if props.get("Caption"):
                caption = props["Caption"][-1]
                caption = "\n".join(translations.get(caption, [caption]))
                assert "\n" not in caption
                color = label_oracle.color565(props.get("CaptionColorNormal", props.get("CaptionColor", ["255,255,255"]))[-1])
                offset = tuple(map(int, props.get("CaptionSme", ["0,0,0,0"])[-1].split(",")[:2]))
                render_text(frame, caption, fonts[props["Font"][-1]],
                            (x + offset[0], y + offset[1], w, h),
                            props.get("CaptionAlignX", ["Center"])[-1],
                            props.get("CaptionAlignY", ["CenterEx"])[-1], color)
            drawn.append((child.name, key))
        elif child.name == "Label":
            text_key = "\n".join(props.get("Text", []))
            lines = translations.get(text_key.strip(), props.get("Text", []))
            assert len(lines) == 1
            text = lines[0]
            color = label_oracle.color565(props.get("TextColor", ["255,255,255"])[-1])
            render_text(frame, text, fonts[props["Font"][-1]], rect,
                        props.get("AlignX", ["Center"])[-1],
                        props.get("AlignY", ["Center"])[-1], color,
                        props.get("WordWrap", ["False"])[-1] == "True")
        elif child.name == "Edit":
            assert not props.get("Text") and not props.get("Image") and props.get("Border", ["False"])[-1] == "False"
        else:
            raise ValueError(f"unexpected M26 showcase control {child.name}")
    frame_bytes = b"".join(struct.pack("<H", value) for value in frame)
    if frame_output is not None:
        frame_output.write_bytes(frame_bytes)
    return {"path": PATH, "nodes": 50, "activated_for_showcase": True,
            "position": (298, 120), "size": (410, 435),
            "control_types": {name: sum(child.name == name for child in node.children) for name in
                              ("Edit", "GraphButton", "Image", "Label")},
            "fonts": {key: {field: font[field] for field in ("resource", "source")}
                      for key, font in sorted(fonts.items())},
            "resources": {key: {field: asset[field] for field in ("path", "source")}
                          for key, asset in sorted(assets.items())},
            "drawn_images": len(drawn), "frame": gi.fp(frame_bytes)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    parser.add_argument("--frame-output", type=Path)
    args = parser.parse_args()
    print(json.dumps(oracle(args.game_root, args.frame_output), ensure_ascii=False, indent=2))
