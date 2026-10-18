"""Independent STRG/ZL03 oracle and native/original storage contract runner."""
import argparse
import ctypes
import hashlib
import json
import mmap
import os
from pathlib import Path
import struct
import subprocess
import sys
import zlib

LIMIT = 256 * 1024 * 1024

def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return f"{value:08x}"

def zl03(data):
    if len(data) < 8 or data[:4] != b"ZL03":
        raise ValueError("bad ZL03 header")
    count = struct.unpack_from("<I", data, 4)[0]
    if not 0 < count <= (LIMIT + 64999) // 65000:
        raise ValueError("bad block count")
    position, result = 8, bytearray()
    for index in range(count):
        if position + 4 > len(data):
            raise ValueError("short block")
        size = struct.unpack_from("<I", data, position)[0]
        position += 4
        if size > len(data) - position:
            raise ValueError("short payload")
        decoder = zlib.decompressobj()
        block = decoder.decompress(data[position:position + size], 65001)
        if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
            raise ValueError("invalid deflate framing")
        if not 0 < len(block) <= 65000 or (index + 1 < count and len(block) != 65000):
            raise ValueError("invalid decoded block size")
        result += block
        if len(result) > LIMIT:
            raise ValueError("decode budget")
        position += size
    if position != len(data):
        raise ValueError("trailing ZL03 bytes")
    return bytes(result)

CRC_TABLE = []
for _index in range(256):
    _entry = _index
    for _bit in range(8):
        _entry = (_entry >> 1) ^ (0xedb88320 if _entry & 1 else 0)
    CRC_TABLE.append(_entry)

def native_crc(state, data):
    # MatrixLib deliberately uses a left shift with this reflected table.
    for byte in data:
        state = ((state << 8) ^ CRC_TABLE[byte ^ (state & 255)]) & 0xffffffff
    return state

def parse_storage(data, *, include_payloads=False):
    if len(data) < 12 or data[:4] != b"STRG":
        raise ValueError("bad STRG header")
    version = struct.unpack_from("<I", data, 4)[0]
    if version > 1:
        raise ValueError("bad version")
    body = zl03(data[8:]) if version else data[8:]
    at = 0
    def u32():
        nonlocal at
        if at + 4 > len(body):
            raise ValueError("short integer")
        value = struct.unpack_from("<I", body, at)[0]
        at += 4
        return value
    def name():
        nonlocal at
        begin = at
        while at + 2 <= len(body):
            unit = body[at:at+2]
            at += 2
            if unit == b"\0\0":
                return body[begin:at-2]
        raise ValueError("unterminated name")
    count = u32()
    if count > len(body) // 6:
        raise ValueError("record count")
    state = 0xffffffff
    records = []
    for _ in range(count):
        table = name()
        state = native_crc(state, table)
        columns = []
        items = u32()
        if items > (len(body) - at) // 10:
            raise ValueError("column count")
        for _ in range(items):
            column = name()
            raw_type, size = u32(), u32()
            kind = raw_type & 0x7fffffff
            if kind > 5 or size > len(body) - at:
                raise ValueError("column header")
            blob = body[at:at+size]
            at += size
            if raw_type & 0x80000000:
                blob = zl03(blob)
            if len(blob) < 12:
                raise ValueError("array header")
            table_at, arrays, element = struct.unpack_from("<III", blob)
            if element != (4, 4, 1, 4, 8, 2)[kind] or table_at < 12 or table_at + arrays * 12 > len(blob):
                raise ValueError("array table")
            lengths, payloads, previous = [], [], 12
            for index in range(arrays):
                offset, length, allocated = struct.unpack_from("<III", blob, table_at + index * 12)
                if offset < previous or length > allocated or offset + allocated * element > table_at:
                    raise ValueError("array bounds")
                lengths.append(length)
                payloads.append(blob[offset:offset + length * element])
                previous = offset + allocated * element
            state = native_crc(state, column)
            state = native_crc(state, blob)
            state = native_crc(state, struct.pack("<I", kind))
            columns.append({"name": column.decode("utf-16-le", "surrogatepass"),
                "type": kind, "lengths": lengths,
                "arrays_sha256": [hashlib.sha256(array).hexdigest() for array in payloads]})
            if include_payloads: columns[-1]["arrays"] = payloads
        records.append({"name": table.decode("utf-16-le", "surrogatepass"), "columns": columns})
    if at != len(body):
        raise ValueError("trailing STRG bytes")
    return {"uid": f"{state:08x}", "records": records}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--package", type=Path)
    parser.add_argument("--entry")
    parser.add_argument("--runner-binary", type=Path)
    parser.add_argument("--qemu-sysroot", type=Path)
    args = parser.parse_args()
    if bool(args.package) != bool(args.entry):
        parser.error("--package and --entry must be supplied together")
    output = args.output.resolve()
    command = [str(args.binary.resolve())]
    if args.runner_binary:
        command += ["-L", str(args.qemu_sysroot.resolve()), str(args.runner_binary.resolve())]
    command.append(str(output))
    if args.package:
        command += [str(args.package.resolve()), args.entry]
    if os.name == "nt":
        ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    result.check_returncode()
    rows = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
    plain = (output / "fixture.strg").read_bytes()
    parsed = parse_storage(plain)
    zipped = parse_storage((output / "fixture-compressed.strg").read_bytes())
    assert parsed == zipped
    assert parsed["uid"] == rows[0]["storage_uid"] and fnv(plain) == rows[0]["plain_fnv32"]
    evidence = {"scope": "STRG/resource CPU contract only", "controlled": rows[0],
        "plain_sha256": hashlib.sha256(plain).hexdigest(), "records": len(parsed["records"])}
    if args.package:
        from probe_ui_images_release import entries, payload
        with args.package.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as blob:
            entry = next(row for row in entries(blob) if row[0] == args.entry)
            source = payload(blob, entry)
        original = parse_storage(source)
        exported = (output / "real-export.strg").read_bytes()
        again = parse_storage(exported)
        assert original["uid"] == rows[1]["real_storage_uid"]
        assert fnv(exported) == rows[1]["real_export_fnv32"]
        assert original["records"] == again["records"]
        evidence["real"] = {"entry": args.entry, "uid": original["uid"],
            "source_bytes": len(source), "source_sha256": hashlib.sha256(source).hexdigest(),
            "export_bytes": len(exported), "export_sha256": hashlib.sha256(exported).hexdigest(),
            "byte_exact_roundtrip": source == exported, "tables": len(original["records"])}
    (output / "oracle.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(json.dumps({"independent_storage_oracle": "PASS", **evidence}))

if __name__ == "__main__":
    main()
