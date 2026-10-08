#!/usr/bin/env python3
"""Read-only, occurrence-aware inventory of release GI and GAI UI resources."""
from __future__ import annotations

import argparse
from collections import Counter
import json
import mmap
from pathlib import Path
import struct

import probe_gai_release as gai_oracle
import probe_m23_text_release as m23
from probe_ui_images_release import KNOWN_CONTROLS, data_entries, decode_dat, entries, payload

BASE_FIELDS = ("Help", "MVUpdate", "MouseBlocking", "MouseBlockingTest", "Enabled")
GAI_FIELDS = ("Image", "ImageFirst", "KindX", "KindY", "AlignX", "AlignY", "PBuf",
              "Stop", "Frame", "FrameLoad", "Auto", "TransColor",
              "SkipImageUpdateRect", "StopAfterOneCycle", "SoundStart")


def key(value: str) -> str:
    return value.replace("\\", "/").casefold()


def inspect_gi(data: bytes) -> dict:
    result = {"source_size": len(data)}
    if len(data) < 64 or data[:2] != b"gi":
        return {**result, "status": "invalid header"}
    version, left, top, right, bottom = struct.unpack_from("<5i", data, 4)
    fmt, planes, clips, clip_offset = struct.unpack_from("<4i", data, 40)
    result.update(version=version, format=fmt, bounds=[left, top, right, bottom],
                  dimensions=[right-left, bottom-top], plane_count=planes,
                  clip_count=clips, clip_offset=clip_offset)
    if (version != 1 or right <= left or bottom <= top or planes < 0 or clips < 0
            or 64 + planes * 32 > len(data) or
            (clips and (clip_offset < 0 or clip_offset + clips * 8 > len(data)))):
        result["status"] = "invalid fields"
        return result
    result["planes"] = []
    for index in range(planes):
        offset, size, pleft, ptop, pright, pbottom = struct.unpack_from("<6i", data, 64 + index * 32)
        result["planes"].append({"offset": offset, "size": size,
                                 "bounds": [pleft, ptop, pright, pbottom]})
        if offset < 0 or size < 0 or (offset and offset + size > len(data)):
            result["status"] = "invalid plane"
            return result
    result["status"] = "Format0" if fmt == 0 else "Format2" if fmt == 2 else "other"
    return result


def inventory(root: Path) -> dict:
    main = m23.parse_blocks(decode_dat(root / "CFG/Main.dat"))
    all_nodes = list(m23.nodes(main))
    styles = {"Style." + n.path.removeprefix("ML/Style/").replace("/", "."): n
              for n in all_nodes if n.path.startswith("ML/Style/")}
    controls = [n for n in all_nodes if n.name in KNOWN_CONTROLS
                and not n.path.startswith("ML/Style/")]
    cache = {key(path.replace("/", ".")): key(name)
             for path, name in data_entries(decode_dat(root / "CFG/CacheData.dat", 0xEA8F3F37))}
    references: dict[str, set[str]] = {}
    gai_controls = []
    base_fields = Counter()
    for node in controls:
        props = m23.effective(m23.style_chain(node, styles))
        base_fields.update(field for field in BASE_FIELDS if field in props)
        for prop, values in props.items():
            for value in values:
                if value.startswith("GI,"):
                    resource = value.split(",", 1)[1].strip()
                    references.setdefault(resource, set()).add(node.unique_path + ":" + prop)
        if node.name == "GI":
            for value in props.get("Image", []):
                references.setdefault(value.strip(), set()).add(node.unique_path + ":Image")
        if node.name == "GAI":
            gai_controls.append({"path": node.unique_path,
                                 "style": props.get("Style", [""])[-1],
                                 "properties": {field: props[field] for field in GAI_FIELDS if field in props},
                                 "base_properties": {field: props[field] for field in BASE_FIELDS if field in props}})
    resources = {}
    gai_resources = {}
    wanted = {cache.get(key(k), key(k)): k for k in references}
    gai_wanted = {cache.get(key(c["properties"].get("Image", [""])[-1]),
                            key(c["properties"].get("Image", [""])[-1]))
                  for c in gai_controls}
    # mmap keeps the 2+ GiB release package set outside Python allocations.
    for package in sorted((root / "DATA").glob("*.pkg")):
        with package.open("rb") as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as mapped:
            for entry in entries(mapped):
                path = key(entry[0])
                if path in wanted:
                    resource = wanted[path]
                    data = payload(mapped, entry)
                    resources[resource] = {"package": package.name, "path": entry[0],
                                           **inspect_gi(data), "references": sorted(references[resource])}
                if path in gai_wanted:
                    source = payload(mapped, entry)
                    try:
                        metadata = gai_oracle.gai(source)
                        frame_formats: Counter[str] = Counter()
                        largest_frame_source = largest_frame_decoded = 0
                        for index in range(metadata["frame_count"]):
                            encoding, gi_bytes, _, _ = gai_oracle.frame(source, metadata, index)
                            largest_frame_source = max(largest_frame_source, len(gi_bytes))
                            if not gi_bytes:
                                frame_formats["empty"] += 1
                                continue
                            try:
                                image = gai_oracle.header(gi_bytes)
                            except ValueError:
                                if metadata["flags"] == 0: raise
                                # Cumulative GAI can carry delta frames without
                                # ordinary standalone GI bounds. It is deferred.
                                frame_formats["cumulative_delta"] += 1
                                continue
                            frame_formats[str(image["format"])] += 1
                            left, top, right, bottom = image["bounds"]
                            largest_frame_decoded = max(largest_frame_decoded,
                                                        (right - left) * (bottom - top) * 4)
                        left, top, right, bottom = metadata["bounds"]
                        gai_resources[path] = {"status": "valid", "source_size": len(source),
                                               "bounds": metadata["bounds"],
                                               "canvas_bgra_bytes": (right - left) * (bottom - top) * 4,
                                               "largest_frame_source": largest_frame_source,
                                               "largest_frame_decoded": largest_frame_decoded,
                                               "frame_count": metadata["frame_count"],
                                               "flags": metadata["flags"],
                                               "sequence_count": metadata["sequence_count"],
                                               "sequence_lengths": [s["frame_count"] for s in metadata["sequences"]],
                                               "frame_formats": dict(sorted(frame_formats.items()))}
                    except ValueError as exc:
                        gai_resources[path] = {"status": "invalid", "source_size": len(source),
                                               "error": str(exc)}
                    for control in gai_controls:
                        raw = control["properties"].get("Image", [""])[-1]
                        if cache.get(key(raw), key(raw)) == path:
                            control["resolved"] = True
                            control["source_size"] = entry[2]
                            control["package"] = package.name
                            control["package_path"] = entry[0]
                            control["container"] = gai_resources[path]
    for resource in references:
        resources.setdefault(resource, {"status": "unresolved", "references": sorted(references[resource])})
    return {"source": "CFG/Main.dat", "read_only": True, "physical_controls": len(controls),
            "gi": {"distinct_keys": len(references),
                   "status_counts": dict(sorted(Counter(r["status"] for r in resources.values()).items())),
                   "largest_source": max((r.get("source_size", 0) for r in resources.values()), default=0),
                   "largest_decoded": max((r.get("dimensions", [0, 0])[0] *
                                           r.get("dimensions", [0, 0])[1] * 4
                                           for r in resources.values()), default=0),
                   "resources": dict(sorted(resources.items()))},
            "gai": {"physical_controls": len(gai_controls),
                    "field_counts": dict(sorted(Counter(field for c in gai_controls
                                                       for field in c["properties"]).items())),
                    "resolved_controls": sum(c.get("resolved", False) for c in gai_controls),
                    "without_pbuf_or_first": sum(c.get("resolved", False) and
                                                 "PBuf" not in c["properties"] and
                                                 "ImageFirst" not in c["properties"]
                                                 for c in gai_controls),
                    "largest_source": max((c.get("source_size", 0) for c in gai_controls), default=0),
                    "largest_frame_source": max((c.get("container", {}).get("largest_frame_source", 0)
                                                 for c in gai_controls), default=0),
                    "largest_frame_decoded": max((c.get("container", {}).get("largest_frame_decoded", 0)
                                                  for c in gai_controls), default=0),
                    "largest_pbuf_canvas_bgra": max((c.get("container", {}).get("canvas_bgra_bytes", 0)
                                                     for c in gai_controls if "PBuf" in c["properties"]), default=0),
                    "flag_counts": dict(sorted(Counter(c["container"]["flags"] for c in gai_controls
                                                       if c.get("container", {}).get("status") == "valid").items())),
                    "controls": gai_controls},
            "base_field_counts": dict(sorted(base_fields.items()))}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("game_root", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = inventory(args.game_root)
    encoded = json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json:
        args.json.write_text(encoded + "\n", encoding="utf-8")
    else:
        print(encoded)
