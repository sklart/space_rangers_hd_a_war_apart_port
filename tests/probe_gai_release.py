"""Independent Python oracle for M16 GI Format-2 in DATA/Asteroid/00.gai."""
from __future__ import annotations
import argparse
import json
import pathlib
import struct
import zlib

HEADER, FRAME, PLANE, RLE = 48, 8, 32, 16
MAX_BYTES = 256 * 1024 * 1024
FNV_OFFSET, FNV_PRIME = 0xCBF29CE484222325, 0x100000001B3


def u32(data, at): return struct.unpack_from("<I", data, at)[0]
def i32(data, at): return struct.unpack_from("<i", data, at)[0]
def fp(data):
    value = FNV_OFFSET
    for byte in data: value = ((value ^ byte) * FNV_PRIME) & 0xffffffffffffffff
    return {"size": len(data), "crc32": f"{zlib.crc32(data):08x}", "fnv64": f"{value:016x}"}


def entries(data, folder, prefix=""):
    _, count, record = struct.unpack_from("<III", data, folder)
    if record != 158: raise ValueError("package record")
    for index in range(count):
        at = folder + 12 + index * record
        name = data[at + 71:at + 134].split(bytes([0]), 1)[0].decode("ascii")
        path, kind = (f"{prefix}/{name}" if prefix else name), i32(data, at + 134)
        if kind == 3: yield from entries(data, u32(data, at + 150), path)
        else: yield path, kind, u32(data, at), u32(data, at + 4), u32(data, at + 150)


def payload(data, kind, size, position):
    at = position + 4
    if kind != 2: return data[at:at + size]
    result = bytearray()
    while len(result) < size:
        stored = u32(data, at); at += 4; block = data[at:at + stored]
        if len(block) != stored or stored < 8 or block[:4] != b"ZL02" or not u32(block, 4): raise ValueError("package ZL02")
        result.extend(zlib.decompress(block[8:])); at += stored
    if len(result) != size: raise ValueError("package payload")
    return bytes(result)


def gai(data):
    if len(data) < HEADER or data[:4] != b"gai\x00": raise ValueError("GAI header")
    version = i32(data, 4); bounds = list(struct.unpack_from("<4i", data, 8)); frames, flags = i32(data, 24), u32(data, 28)
    sequence_offset, sequence_size = struct.unpack_from("<2i", data, 32)
    if version != 1 or not 0 < frames <= 100000 or bounds[2] <= bounds[0] or bounds[3] <= bounds[1] or HEADER + frames * FRAME > len(data): raise ValueError("GAI fields")
    sequences = []
    if sequence_offset:
        if sequence_offset < 0 or sequence_size < 8 or sequence_offset + sequence_size > len(data): raise ValueError("sequence table")
        count = i32(data, sequence_offset)
        if not 0 <= count <= 100000 or 8 + count * 8 > sequence_size: raise ValueError("sequence directory")
        for index in range(count):
            block = i32(data, sequence_offset + 8 + index * 8)
            if block < 0 or block + 4 > sequence_size: raise ValueError("sequence block")
            count2 = i32(data, sequence_offset + block)
            if not 0 <= count2 <= 100000 or block + 4 + count2 * 8 > sequence_size: raise ValueError("sequence frames")
            pairs = [struct.unpack_from("<ii", data, sequence_offset + block + 4 + step * 8) for step in range(count2)]
            if any(source < 0 or source >= frames for source, _ in pairs): raise ValueError("sequence source")
            sequences.append({"frame_count": count2, "source_indices": [item[0] for item in pairs], "frame_delays": [item[1] for item in pairs]})
    elif sequence_size: raise ValueError("sequence size")
    return {"version": version, "bounds": bounds, "frame_count": frames, "flags": flags, "sequence_count": len(sequences), "sequences": sequences}


def frame(data, meta, index):
    offset, size = struct.unpack_from("<ii", data, HEADER + index * FRAME)
    if offset == 0: return "EMPTY", b"", offset, size
    if offset < 0 or size <= 0 or offset + size > len(data): raise ValueError("frame range")
    blob = data[offset:offset + size]
    if blob[:2] == b"gi": return "RAW_GI", blob, offset, size
    if len(blob) < 8 or blob[:2] != b"ZL" or blob[:4] not in (b"ZL01", b"ZL02") or not u32(blob, 4): raise ValueError("frame encoding")
    decoded = zlib.decompress(blob[8:])
    if len(decoded) != u32(blob, 4) or len(decoded) > MAX_BYTES: raise ValueError("frame inflate")
    return blob[:4].decode("ascii"), decoded, offset, size


def header(data):
    if len(data) < 64 or data[:2] != b"gi" or i32(data, 4) != 1: raise ValueError("GI header")
    bounds = list(struct.unpack_from("<4i", data, 8)); masks = list(struct.unpack_from("<4I", data, 24))
    fmt, planes, clips, clip_offset = struct.unpack_from("<4i", data, 40)
    if bounds[2] <= bounds[0] or bounds[3] <= bounds[1] or planes < 0 or clips < 0 or 64 + planes * PLANE > len(data): raise ValueError("GI fields")
    if clips and (clip_offset < 0 or clip_offset + clips * 8 > len(data)): raise ValueError("GI clips")
    return {"version": 1, "format": fmt, "bounds": bounds, "planes": planes, "clips": clips, "masks": [f"{value:08x}" for value in masks]}


def plane(data, image, index):
    offset, size, left, top, right, bottom = struct.unpack_from("<6i", data, 64 + index * PLANE)
    if offset == 0: return None
    if offset < 0 or size < RLE or offset > len(data) or size > len(data) - offset: raise ValueError("plane range")
    stream_bytes, width, height, info = struct.unpack_from("<iiiI", data, offset)
    if stream_bytes < 0 or width <= 0 or height <= 0 or RLE + stream_bytes > size: raise ValueError("RLE header")
    x, y = left - image["bounds"][0], top - image["bounds"][1]
    iw, ih = image["bounds"][2] - image["bounds"][0], image["bounds"][3] - image["bounds"][1]
    if x < 0 or y < 0 or x + width > iw or y + height > ih: raise ValueError("RLE placement")
    return {"bounds": [left, top, right, bottom], "origin": [x, y], "size": size, "stream_bytes": stream_bytes, "rle_width": width, "rle_height": height, "format_info": info, "stream": data[offset + RLE:offset + RLE + stream_bytes]}


def validate(stream, width, height, literal_size):
    at = 0
    for _ in range(height):
        x = 0
        while True:
            if at >= len(stream): raise ValueError("RLE rows")
            command = stream[at]; at += 1
            if command == 0:
                if x != width: raise ValueError("RLE short row")
                break
            if command == 128:
                if x: raise ValueError("RLE blank row")
                break
            count = command & 127
            if x + count > width: raise ValueError("RLE width")
            if command & 128:
                size = count * literal_size
                if size > len(stream) - at: raise ValueError("RLE literal")
                at += size
            x += count
    if at != len(stream): raise ValueError("RLE trailing")


def draw(pixels, pitch, data, mode):
    stream, width, height = data["stream"], data["rle_width"], data["rle_height"]
    validate(stream, width, height, 1 if mode == 2 else 2)
    base_x, base_y, at = data["origin"][0], data["origin"][1], 0
    for row in range(height):
        x = 0
        while True:
            command = stream[at]; at += 1
            if command == 0 or command == 128: break
            count = command & 127
            if command < 128: x += count; continue
            for _ in range(count):
                target = (base_y + row) * pitch + (base_x + x) * 4
                if mode == 2: pixels[target + 3] = 252 - 4 * stream[at]; at += 1
                else:
                    value = struct.unpack_from("<H", stream, at)[0]; at += 2
                    blue, green, red = (value & 31) << 3, (value >> 3) & 252, (value >> 8) & 248
                    if mode == 1 and pixels[target + 3]:
                        alpha = pixels[target + 3]; blue, green, red = blue * 255 // alpha, green * 255 // alpha, red * 255 // alpha
                    pixels[target:target + 3] = bytes((blue & 255, green & 255, red & 255))
                    if mode == 0: pixels[target + 3] = 255
                x += 1


def decode2(data):
    meta = header(data)
    if meta["format"] != 2 or meta["planes"] < 3: raise ValueError("not Format-2")
    width, height = meta["bounds"][2] - meta["bounds"][0], meta["bounds"][3] - meta["bounds"][1]
    pitch = width * 4
    if pitch * height > MAX_BYTES: raise ValueError("output limit")
    planes = [plane(data, meta, index) for index in range(3)]
    pixels = bytearray(pitch * height)
    for index in (2, 1, 0):
        if planes[index] is not None: draw(pixels, pitch, planes[index], index)
    meta["decoded"] = {"width": width, "height": height, "bpp": 4, "pitch": pitch}
    meta["plane_details"] = [None if item is None else {key: value for key, value in item.items() if key != "stream"} for item in planes]
    return meta, bytes(pixels)


def record(index, meta, pixels):
    left, top, right, bottom = meta["bounds"]; decoded = meta["decoded"]
    return struct.pack("<IiiiiIIII", index, left, top, right, bottom, decoded["width"], decoded["height"], decoded["pitch"], len(pixels)) + pixels


def analyze(path, source):
    meta, formats, rows, aggregate = gai(source), {str(index): 0 for index in range(7)}, [], bytearray()
    for index in range(meta["frame_count"]):
        encoding, gi, offset, stored = frame(source, meta, index)
        if not gi: continue
        gi_meta = header(gi); formats[str(gi_meta["format"])] = formats.get(str(gi_meta["format"]), 0) + 1
        if gi_meta["format"] != 2: continue
        decoded, pixels = decode2(gi)
        rows.append({"index": index, "offset": offset, "stored_size": stored, "encoding": encoding, "gi": fp(gi) | decoded, "pixels": fp(pixels)})
        aggregate.extend(record(index, decoded, pixels))
    if not rows: raise ValueError("no Format-2 frames")
    sequence = meta["sequences"][0]
    delays, sources = sequence["frame_delays"], sequence["source_indices"]
    if not delays or len(delays) != len(sources): raise ValueError("sequence 0")
    sequence_bytes = struct.pack("<II", 0, len(delays)) + b"".join(struct.pack("<ii", source_index, delay) for source_index, delay in zip(sources, delays))
    decoded = {row["index"]: row for row in rows}
    cycle = bytearray()
    for position, (source_index, delay) in enumerate(zip(sources, delays)):
        row = decoded[source_index]; gi = header(frame(source, meta, source_index)[1]); image, pixels = decode2(frame(source, meta, source_index)[1])
        info = image["decoded"]
        cycle.extend(struct.pack("<IIIIIII", position, source_index, delay, info["width"], info["height"], info["pitch"], len(pixels)))
        cycle.extend(pixels)
    m17 = {"sequence_index": 0, "frame_count": len(delays), "source_indices": sources, "frame_delays": delays,
           "delay_min": min(delays), "delay_max": max(delays), "nominal_cycle_ms": sum(delays),
           "zero_delays": sum(delay == 0 for delay in delays), "negative_delays": sum(delay < 0 for delay in delays),
           "sequence_fingerprint": fp(sequence_bytes), "cycle_fingerprint": fp(bytes(cycle))}
    return {"resource": path, "gai": fp(source) | meta, "gi_formats": formats, "format2": {"decoded_frames": len(rows), "frame0": rows[0], "aggregate": fp(bytes(aggregate))}, "m17": m17}


def main():
    parser = argparse.ArgumentParser(); parser.add_argument("game_root", type=pathlib.Path); args = parser.parse_args()
    package = (args.game_root / "DATA" / "common.pkg").read_bytes()
    match = next((item for item in entries(package, u32(package, 0)) if item[0].upper() == "DATA/ASTEROID/00.GAI"), None)
    if not match: raise ValueError("missing DATA/Asteroid/00.gai")
    print(json.dumps(analyze(match[0], payload(package, match[1], match[3], match[4])), indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
