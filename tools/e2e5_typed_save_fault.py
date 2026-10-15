"""Create an isolated RSG64 fixture with a missing typed ship ID.

Only accepts a QuickSave.sav produced by e2e5_host_game_probe.py under its
ignored runs directory. The source is read-only; the output is a new run.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
from uuid import uuid4
import zlib


ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / "port/switch/build/e2e5-host-game/runs"


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def cipher(data: bytes, seed: int) -> bytes:
    if not 0 < seed < 0x7fffffff:
        raise ValueError("unsupported save cipher seed")
    result = bytearray(data)
    state = seed
    for i in range(len(result)):
        state = 16807 * (state % 127773) - 2836 * (state // 127773)
        if state <= 0:
            state += 0x7fffffff
        result[i] ^= (state - 1) & 0xff
    return bytes(result)


def mutate(source: Path) -> tuple[bytes, int]:
    data = source.read_bytes()
    offset = 0
    fields = []
    for _ in range(8):
        end = offset
        while end + 1 < len(data) and data[end:end + 2] != b"\x00\x00":
            end += 2
        if end + 1 >= len(data) or end - offset > 4096:
            raise ValueError("invalid UTF-16 save header")
        fields.append(data[offset:end].decode("utf-16le"))
        offset = end + 2
    if fields[0:2] != ["RSG64", "v168"] or fields[7] != "EZ":
        raise ValueError("requires a typed RSG64/v168 save")
    for _ in range(2):
        length = u32(data, offset)
        offset += 4 + length
        if offset > len(data):
            raise ValueError("preview exceeds save length")
    block_header = offset
    old_crc, seed, length = struct.unpack_from("<III", data, offset)
    offset += 12
    if not length or offset + length > len(data):
        raise ValueError("state block exceeds save length")
    packed = cipher(data[offset:offset + length], seed)
    if zlib.crc32(packed) != old_crc:
        raise ValueError("source save CRC mismatch")
    state = bytearray(zlib.decompress(packed))
    marker = "CurShip".encode("utf-16le") + b"\x00\x00" + bytes((13,))
    matches = []
    cursor = 0
    while (index := state.find(marker, cursor)) >= 0:
        id_offset = index + len(marker)
        if id_offset + 4 <= len(state):
            matches.append((id_offset, u32(state, id_offset)))
        cursor = index + len(marker)
    candidates = [(where, value) for where, value in matches if value != 0]
    if not candidates:
        raise ValueError("no typed non-null CurShip reference in game state")
    where, original_id = candidates[-1]
    struct.pack_into("<I", state, where, 0xfffffffe)
    repacked = zlib.compress(state)
    replacement = struct.pack("<III", zlib.crc32(repacked), seed,
                              len(repacked)) + cipher(repacked, seed)
    return data[:block_header] + replacement + data[offset + length:], original_id


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    if source.name != "QuickSave.sav" or RUNS.resolve() not in source.parents:
        parser.error("source must be an isolated probe QuickSave.sav")
    mutated, original_id = mutate(source)
    target = RUNS / uuid4().hex / "user/config/save/QuickSave.sav"
    target.parent.mkdir(parents=True)
    target.write_bytes(mutated)
    print(f"source_ship_id={original_id} replacement_id=4294967294")
    print(f"fixture={target}")
    print(f"sha256={hashlib.sha256(mutated).hexdigest()}")


if __name__ == "__main__":
    main()
