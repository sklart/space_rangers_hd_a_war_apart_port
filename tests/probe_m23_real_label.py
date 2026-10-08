#!/usr/bin/env python3
"""Independent RGB565 oracle for the lexicographically selected release Label."""
from __future__ import annotations

import argparse
import json
import pathlib
import struct

import probe_aft_release as aft
import probe_m23_text_release as inventory
import probe_m23_ui_text as synthetic_oracle
from probe_ui_images_release import data_entries, decode_dat, entries, payload


def resolve_font(root: pathlib.Path, key: str) -> tuple[str, str, bytes]:
    cache = {name.replace("/", ".").casefold(): path.replace("\\", "/")
             for name, path in data_entries(decode_dat(root / "CFG/CacheData.dat", 0xEA8F3F37))}
    path = cache[key.casefold()]
    for package in sorted((root / "DATA").glob("*.pkg")):
        blob = package.read_bytes()
        for item in entries(blob):
            if item[0].replace("\\", "/").casefold() == path.casefold():
                return package.name, item[0], payload(blob, item)
    raise ValueError(f"font resource not found: {key} -> {path}")


def glyphs(source: bytes) -> dict[str, tuple[int, list[tuple[int, int, int, int, bytes]]]]:
    result = {}
    count = struct.unpack_from("<I", source, 8)[0]
    for index in range(count):
        at = 32 + index * 64
        code, advance_a, advance_b, advance_c = struct.unpack_from("<Iiii", source, at)
        planes = []
        for plane_at in (at + 16, at + 40):
            left, top, width, height, offset, size = struct.unpack_from("<iiiiII", source, plane_at)
            if offset:
                planes.append((left, top, width, height, source[offset:offset + size]))
        result[chr(code)] = (advance_a + advance_b + advance_c, planes)
    return result


def mask_pixels(encoded: bytes, width: int, height: int):
    """Decode the opaque OKGF mask without calling the C++/C decoder."""
    at = 16
    for y in range(height):
        x = 0
        while True:
            command = encoded[at]
            at += 1
            if command in (0, 128):
                break
            count = command & 127
            if command & 128:
                for offset in range(count):
                    yield x + offset, y
            x += count


def color565(value: str) -> int:
    r, g, b = (int(part.strip()) & 255 for part in value.split(","))
    # GR_GraphBuf.TPixelFormatGR::PackNormalizedRgb truncates each channel
    # after scaling by RedLevels-1 / GreenLevels-1 / BlueLevels-1.
    return ((r * 31 // 255) << 11) | ((g * 63 // 255) << 5) | (b * 31 // 255)


def release_baseline(root: pathlib.Path, language: str) -> dict:
    report = inventory.inventory(root, language)
    selected = report["label"]["selected_real_baseline"]
    if selected is None:
        raise ValueError("no eligible release Label")
    text = selected["text"]
    if "<" in text or "\n" in text:
        raise ValueError("selected release Label requires tagged or multiline oracle")
    all_nodes = list(inventory.nodes(inventory.parse_blocks(decode_dat(root / "CFG/Main.dat"))))
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in all_nodes if node.path.startswith("ML/Style/")}
    label = next(node for node in all_nodes if node.unique_path == selected["path"])
    props = inventory.effective(inventory.style_chain(label, styles))
    px, py, *_ = (int(value.strip()) for value in props["Pos"][-1].split(","))
    width, height = (int(value.strip()) for value in props["Size"][-1].split(","))
    if props.get("AlignX", ["Center"])[-1] != "Center" or props.get("AlignY", ["Center"])[-1] != "CenterEx":
        raise ValueError("selected release Label alignment changed")
    shadow = int(props.get("TextShadow", ["0"])[-1])
    border = int(props.get("TextBorder", ["0"])[-1])
    if border != 0 or props.get("Image") or props.get("Border"):
        raise ValueError("selected release Label draw passes changed")
    package, resource, source = resolve_font(root, selected["font_key"])
    metrics = aft.aft(source)
    lookup = glyphs(source)
    pen = 0
    left, right, top, bottom = 2**31 - 1, -(2**31), 2**31 - 1, -(2**31)
    placements = []
    for char in text:
        if char not in lookup:
            raise ValueError(f"missing selected glyph U+{ord(char):04X}")
        advance, planes = lookup[char]
        placements.append((pen, planes))
        for plane_left, plane_top, plane_width, plane_height, _ in planes:
            left = min(left, pen + plane_left)
            right = max(right, pen + plane_left + plane_width)
            top = min(top, plane_top)
            bottom = max(bottom, plane_top + plane_height)
        pen += advance
    right = max(right, pen)
    bounds = [left, top, right, bottom]
    extra = border + max(border, shadow)
    content_size = [right - left + extra, bottom - top + extra + 2]
    text_x = px + width // 2 - (right - left) // 2
    baseline = py + height // 2 - metrics["centering_height"] // 2 + metrics["centering_height"]
    fb_width, fb_height = max(1024, px + width), max(60, py + height)
    pixels = [0] * (fb_width * fb_height)
    clip = (px, py, px + width, py + height)

    def draw(dx: int, dy: int, color: int) -> None:
        for position, planes in placements:
            for plane_left, plane_top, plane_width, plane_height, encoded in planes:
                for gx, gy in mask_pixels(encoded, plane_width, plane_height):
                    x = text_x + position + plane_left + gx + dx
                    y = baseline + plane_top + gy + dy
                    if clip[0] <= x < clip[2] and clip[1] <= y < clip[3] and 0 <= x < fb_width and 0 <= y < fb_height:
                        pixels[y * fb_width + x] = color

    if shadow > 0:
        draw(shadow, shadow, color565(props.get("TextShadowColor", ["0,0,0"])[-1]))
    draw(0, 0, color565(props.get("TextColor", ["255,255,255"])[-1]))
    changed = [(index % fb_width, index // fb_width, value)
               for index, value in enumerate(pixels) if value]
    pixel_bounds = ([min(item[0] for item in changed), min(item[1] for item in changed),
                     max(item[0] for item in changed) + 1, max(item[1] for item in changed) + 1]
                    if changed else [0, 0, 0, 0])
    region = struct.pack("<" + "H" * len(pixels), *pixels)
    tree = synthetic_oracle.Node("m23-root", 1, (0, 0), (0, 0),
                                 (fb_width, fb_height))
    tree.children.append(synthetic_oracle.Node("WinText", 4, (px, py), (px, py),
                                                (width, height), depth=8))
    tree_crc, tree_fnv = synthetic_oracle.fingerprint(synthetic_oracle.tree_bytes(tree))
    return {"path": selected["path"], "font_key": selected["font_key"],
            "package": package, "resource": resource, "source_size": len(source),
            "source": aft.fingerprint(source), "structural": metrics["structural"],
            "metrics": {key: metrics[key] for key in ("glyph_count", "line_height", "centering_height",
                                                       "above_baseline", "below_baseline", "max_advance")},
            "text": text, "text_utf16": aft.fingerprint(text.encode("utf-16le")),
            "position": [px, py], "size": [width, height], "bounds": bounds,
            "content_size": content_size, "line_count": 1,
            "text_x": text_x, "baseline_y": baseline,
            "changed_pixels": len(changed), "pixel_bounds": pixel_bounds,
            "first_pixels": changed[:16],
            "frame_size": [fb_width, fb_height], "frame": aft.fingerprint(region),
            "tree": {"crc32": tree_crc, "fnv64": tree_fnv}}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", nargs="?", type=pathlib.Path)
    parser.add_argument("--language", default="Rus")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        assert color565("255,255,230") == 0xFFFB
        encoded = struct.pack("<IIII", 4, 2, 2, 0) + b"\x82\x00\x82\x00"
        assert list(mask_pixels(encoded, 2, 2)) == [(0, 0), (1, 0), (0, 1), (1, 1)]
        print("M23 REAL LABEL ORACLE SELF-TEST PASS")
        return
    if args.game_root is None:
        parser.error("game_root is required outside --self-test")
    print(json.dumps(release_baseline(args.game_root, args.language), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
