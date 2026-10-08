#!/usr/bin/env python3
"""Read-only, occurrence-aware M23 Label and tagged-text release inventory."""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass, field
import json
import pathlib
import re
import struct

import probe_aft_release as aft_probe
import probe_m22_ui_release as m22_probe
from probe_ui_images_release import KNOWN_CONTROLS, block_entries, data_entries, decode_dat, entries, payload

SUPPORTED = frozenset((*m22_probe.SUPPORTED, "Label"))
LABEL_PROPERTIES = frozenset((
    "Style", "Pos", "PosZ", "Size", "Sme", "Name", "Active", "Font", "Text",
    "Image", "ImageKindX", "ImageKindY", "TextColor", "Border", "BorderLightColor",
    "BorderDarkColor", "WordWrap", "AlignX", "AlignY", "TextBorder",
    "TextBorderColor", "TextShadow", "TextShadowColor",
))
TAG = re.compile(r"<([^<>]*)>")
KNOWN_TAGS = frozenset(("color", "/color", "object", "fix", "/fix", "format",
                        "/format", "td", "align", "/align"))


@dataclass
class Node:
    name: str
    path: str
    unique_path: str
    parent: Node | None
    properties: list[tuple[str, str]] = field(default_factory=list)
    children: list[Node] = field(default_factory=list)

    def values(self, name: str) -> list[str]:
        return [value for key, value in self.properties if key == name]

    def last(self, name: str, default: str = "") -> str:
        values = self.values(name)
        return values[-1] if values else default


def parse_blocks(decoded: bytes) -> Node:
    """Keep duplicate sibling blocks distinct; block_entries() only yields paths."""
    root = Node("", "", "", None)

    def wide(at: int) -> tuple[str, int]:
        end = at
        while end + 1 < len(decoded) and decoded[end:end + 2] != b"\0\0":
            end += 2
        if end + 1 >= len(decoded):
            raise ValueError("unterminated UTF-16 block value")
        return decoded[at:end].decode("utf-16le", "strict"), end + 2

    def walk(at: int, parent: Node) -> int:
        if at + 5 > len(decoded):
            raise ValueError("truncated block header")
        sorted_index = decoded[at]
        count = struct.unpack_from("<I", decoded, at + 1)[0]
        at += 5
        if sorted_index not in (0, 1) or count > 1_000_000:
            raise ValueError("invalid block header")
        sibling_counts: Counter[str] = Counter()
        for _ in range(count):
            if sorted_index:
                if at + 8 > len(decoded):
                    raise ValueError("truncated sorted index")
                at += 8
            if at >= len(decoded):
                raise ValueError("truncated entry")
            kind = decoded[at]
            at += 1
            name, at = wide(at)
            if kind == 1:
                value, at = wide(at)
                parent.properties.append((name, value))
            elif kind == 2:
                ordinal = sibling_counts[name]
                sibling_counts[name] += 1
                path = f"{parent.path}/{name}" if parent.path else name
                unique = f"{parent.unique_path}/{name}#{ordinal}" if parent.unique_path else f"{name}#{ordinal}"
                child = Node(name, path, unique, parent)
                parent.children.append(child)
                at = walk(at, child)
            elif kind != 0:
                raise ValueError(f"unknown block entry kind {kind}")
        return at

    if walk(0, root) != len(decoded):
        raise ValueError("trailing block bytes")
    return root


def nodes(root: Node):
    for child in root.children:
        yield child
        yield from nodes(child)


def style_chain(label: Node, style_index: dict[str, Node]) -> list[Node]:
    result: list[Node] = []
    seen: set[str] = set()
    style = label.last("Style")
    while style:
        if style in seen or len(result) >= 32:
            raise ValueError(f"cyclic Label style at {label.unique_path}")
        seen.add(style)
        current = style_index.get(style)
        if current is None:
            raise ValueError(f"missing Label style {style} at {label.unique_path}")
        result.append(current)
        style = current.last("Style")
    return list(reversed(result)) + [label]


def effective(chain: list[Node]) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    for source in chain:
        grouped: dict[str, list[str]] = defaultdict(list)
        for name, value in source.properties:
            grouped[name].append(value)
        result.update(grouped)
    return result


def tags(text: str) -> tuple[Counter[str], Counter[str]]:
    found: Counter[str] = Counter()
    unknown: Counter[str] = Counter()
    for match in TAG.finditer(text):
        if match.start() and text[match.start() - 1] == "<":
            continue
        body = match.group(1)
        name = body.partition("=")[0].lower()
        found[name] += 1
        if name not in KNOWN_TAGS:
            unknown[body] += 1
    return found, unknown


def visible_text(text: str) -> str:
    # Release tags are validated separately; their parameter characters are not glyphs.
    return TAG.sub("", text)


def image_reference(node: Node, styles: dict[str, Node]) -> tuple[str, str]:
    props = effective(style_chain(node, styles))
    raw = props.get("Image", [""])[-1].strip()
    if node.name == "Image":
        mode, separator, key = raw.partition(",")
        if not separator:
            return "Simple", raw
        return mode.strip(), key.strip()
    return node.name.removesuffix("Image"), raw


def language_map(path: pathlib.Path) -> dict[str, list[str]]:
    result: dict[str, list[str]] = defaultdict(list)
    for path_name, _, value in block_entries(decode_dat(path)):
        result[path_name.replace("/", ".")].append(value)
    return result


def font_sources(game_root: pathlib.Path, keys: set[str]) -> dict[str, dict]:
    cache = {path.replace("/", ".").casefold(): name.replace("\\", "/")
             for path, name in data_entries(decode_dat(game_root / "CFG/CacheData.dat", 0xEA8F3F37))}
    paths = {cache[key.casefold()].casefold() for key in keys if key.casefold() in cache}
    found: dict[str, dict] = {}
    for package in sorted((game_root / "DATA").glob("*.pkg")):
        blob = package.read_bytes()
        for item in entries(blob):
            name = item[0].replace("\\", "/").casefold()
            if name not in paths or name in found:
                continue
            source = payload(blob, item)
            metrics = aft_probe.aft(source)
            codes = {struct.unpack_from("<I", source, 32 + index * 64)[0]
                     for index in range(metrics["glyph_count"])}
            found[name] = {"resource": item[0], "package": package.name,
                           "source_size": len(source), "source": aft_probe.fingerprint(source),
                           "structural": metrics["structural"], "codes": codes}
    return {key: found.get(cache.get(key.casefold(), "").casefold(), {}) for key in keys}


def inventory(game_root: pathlib.Path, language: str) -> dict:
    main = decode_dat(game_root / "CFG/Main.dat")
    root = parse_blocks(main)
    all_nodes = list(nodes(root))
    controls = [node for node in all_nodes if node.name in KNOWN_CONTROLS]
    labels = [node for node in controls if node.name == "Label"]
    styles = {"Style." + node.path.removeprefix("ML/Style/").replace("/", "."): node
              for node in all_nodes if node.path.startswith("ML/Style/")}
    translations = language_map(game_root / "CFG" / language / "Lang.dat")
    prepared = []
    font_keys: set[str] = set()
    direct_fonts = inherited_fonts = 0
    style_counts: Counter[str] = Counter()
    align_x: Counter[str] = Counter()
    align_y: Counter[str] = Counter()
    properties: Counter[str] = Counter()
    tag_counts: Counter[str] = Counter()
    unknown_tags: Counter[str] = Counter()
    for label in labels:
        chain = style_chain(label, styles)
        props = effective(chain)
        for key in props:
            properties[key] += 1
        style_counts[label.last("Style", "(none)")] += 1
        direct_fonts += bool(label.values("Font"))
        inherited_fonts += bool(not label.values("Font") and props.get("Font"))
        font_key = props.get("Font", [""])[-1]
        if font_key:
            font_keys.add(font_key)
        align_x[props.get("AlignX", ["Center"])[-1]] += 1
        align_y[props.get("AlignY", ["Center"])[-1]] += 1
        original = props.get("Text", [])
        lookup = "\n".join(original).strip()
        resolved = translations.get(lookup, original)
        text = "\n".join(resolved)
        found, unknown = tags(text)
        tag_counts.update(found)
        unknown_tags.update(unknown)
        prepared.append((label, props, font_key, text, bool(lookup in translations)))
    fonts = font_sources(game_root, font_keys)
    missing = Counter()
    missing_examples: dict[str, list[str]] = {}
    eligible_labels = []
    for label, props, font_key, text, localized in prepared:
        codes = fonts.get(font_key, {}).get("codes")
        if codes is None:
            missing[font_key or "(no font)"] += 1
            continue
        absent = sorted({ord(char) for char in visible_text(text) if char not in "\r\n" and
                         ord(char) not in codes})
        if absent:
            missing[font_key] += 1
            missing_examples.setdefault(font_key, [f"U+{code:04X}" for code in absent[:12]])
        found_tags, unknown = tags(text)
        if (text and not absent and not unknown and "object" not in found_tags and
                set(props) <= LABEL_PROPERTIES and not props.get("Image")):
            eligible_labels.append({"path": label.unique_path, "font_key": font_key,
                                    "text": text, "localized": localized,
                                    "text_utf16": aft_probe.fingerprint(text.encode("utf-16le"))})
    selected_label = min(eligible_labels, key=lambda item: item["path"].casefold()) if eligible_labels else None

    cache = {path.replace("/", ".").casefold(): name.replace("\\", "/").casefold()
             for path, name in data_entries(decode_dat(game_root / "CFG/CacheData.dat", 0xEA8F3F37))}
    available: set[str] = set()
    for package in sorted((game_root / "DATA").glob("*.pkg")):
        available.update(item[0].replace("\\", "/").casefold()
                         for item in entries(package.read_bytes()))

    def backing_exists(key: str) -> bool:
        normalized = key.replace("\\", "/").casefold()
        return cache.get(normalized, normalized) in available

    def resource_ok(node: Node) -> bool:
        if node.name == "Label":
            props = effective(style_chain(node, styles))
            font_key = props.get("Font", [""])[-1]
            if not fonts.get(font_key) or not set(props) <= LABEL_PROPERTIES:
                return False
            text = "\n".join(props.get("Text", []))
            text = "\n".join(translations.get(text.strip(), props.get("Text", [])))
            found_tags, unknown = tags(text)
            if unknown or "object" in found_tags:
                return False
            image = props.get("Image", [""])[-1].strip()
            if image:
                parts = [part.strip() for part in image.split(",")]
                if len(parts) == 1:
                    mode, key = "Simple", parts[0]
                elif len(parts) == 2:
                    mode, key = parts
                else:
                    return False
                if mode not in ("Simple", "Trans", "Alpha") or not key or not backing_exists(key):
                    return False
            return True
        if node.name not in ("Image", "SimpleImage", "TransImage", "AlphaImage"):
            return node.name == "Panel"
        mode, key = image_reference(node, styles)
        if mode not in ("Simple", "Trans", "Alpha") or not key:
            return False
        return backing_exists(key)

    subtrees = []
    blocker_roots: Counter[str] = Counter()
    blocker_occurrences: Counter[str] = Counter()
    for root_node in (node for node in controls if node.name == "Panel"):
        descendants = [root_node, *(node for node in nodes(root_node) if node.name in KNOWN_CONTROLS)]
        visual = [node for node in descendants if node.name != "Panel"]
        depth = max((node.unique_path.count("/") - root_node.unique_path.count("/") + 1
                     for node in descendants), default=0)
        if depth < 2 or len(visual) < 3:
            continue
        blockers = Counter(node.name for node in descendants if node.name not in SUPPORTED)
        if blockers:
            blocker_roots.update(blockers.keys())
            blocker_occurrences.update(blockers)
            continue
        if all(resource_ok(node) for node in visual):
            subtrees.append({"path": root_node.unique_path, "nodes": len(descendants),
                             "depth": depth, "visual_leaves": len(visual),
                             "controls": dict(sorted(Counter(node.name for node in descendants).items()))})
    selected_subtree = min(subtrees, key=lambda item: item["path"].casefold()) if subtrees else None
    legacy_blocks, _ = m22_probe.paths_for(main)
    legacy_controls = [path for path in legacy_blocks if path.rsplit("/", 1)[-1] in KNOWN_CONTROLS]
    return {
        "source": str(game_root / "CFG/Main.dat"), "language": language, "read_only": True,
        "m22_comparable": {"total_unique_control_paths_with_properties": len(legacy_controls),
                           "supported_before": sum(path.rsplit("/", 1)[-1] in m22_probe.SUPPORTED for path in legacy_controls),
                           "supported_after": sum(path.rsplit("/", 1)[-1] in SUPPORTED for path in legacy_controls)},
        "occurrences": {"total_controls": len(controls), "labels": len(labels),
                        "supported_before": sum(node.name in m22_probe.SUPPORTED for node in controls),
                        "supported_after": sum(node.name in SUPPORTED for node in controls)},
        "label": {"styles": dict(sorted(style_counts.items())), "direct_font": direct_fonts,
                  "inherited_font": inherited_fonts, "font_keys": sorted(font_keys),
                  "align_x": dict(sorted(align_x.items())), "align_y": dict(sorted(align_y.items())),
                  "word_wrap": properties["WordWrap"], "border": properties["Border"],
                  "text_border": properties["TextBorder"], "text_shadow": properties["TextShadow"],
                  "embedded_image": properties["Image"],
                  "property_counts": dict(sorted(properties.items())),
                  "unknown_properties": {key: count for key, count in sorted(properties.items())
                                         if key not in LABEL_PROPERTIES},
                  "localized": sum(item[4] for item in prepared),
                  "tag_counts": dict(sorted(tag_counts.items())),
                  "unknown_tags": dict(sorted(unknown_tags.items())),
                  "missing_glyph_labels_by_font": dict(sorted(missing.items())),
                  "missing_glyph_examples": missing_examples,
                  "eligible_real_baseline_count": len(eligible_labels),
                  "selected_real_baseline": selected_label},
        "real_supported_subtree": selected_subtree or "NOT FOUND",
        "blocking_control_ranking": [
            {"control": name, "candidate_subtrees_blocked": count,
             "occurrences": blocker_occurrences[name]}
            for name, count in blocker_roots.most_common()],
        "fonts": {key: {name: value for name, value in detail.items() if name != "codes"}
                  for key, detail in sorted(fonts.items())},
    }


def self_test() -> None:
    def wide(value: str) -> bytes:
        return value.encode("utf-16le") + b"\0\0"

    def parameter(name: str, value: str) -> bytes:
        return b"\x01" + wide(name) + wide(value)

    def block(name: str, members: list[bytes]) -> bytes:
        return b"\x02" + wide(name) + b"\x00" + struct.pack("<I", len(members)) + b"".join(members)

    decoded = b"\x00" + struct.pack("<I", 1) + block("Panel", [
        block("Label", [parameter("Text", "A"), parameter("Text", "B")]),
        block("Label", [parameter("Text", "C")]),
    ])
    root = parse_blocks(decoded)
    labels = [node for node in nodes(root) if node.name == "Label"]
    assert len(labels) == 2
    assert labels[0].unique_path != labels[1].unique_path
    assert labels[0].values("Text") == ["A", "B"]
    assert labels[1].values("Text") == ["C"]
    assert effective([labels[0], labels[1]])["Text"] == ["C"]
    assert tags("<<literal <color=1,2,3>A</color>") == (
        Counter({"color": 1, "/color": 1}), Counter())


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", nargs="?", type=pathlib.Path)
    parser.add_argument("--language", default="Eng")
    parser.add_argument("--json", type=pathlib.Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        print("M23 TEXT INVENTORY SELF-TEST PASS")
        return
    if args.game_root is None:
        parser.error("game_root is required outside --self-test")
    result = inventory(args.game_root, args.language)
    output = json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(output, encoding="utf-8")
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
