#!/usr/bin/env python3
"""Independent, read-only M21 oracle for the selected release Simple PNG.

The script intentionally implements package extraction and indexed PNG decoding
locally.  It has no dependency on the portable C++ image decoder or renderer.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import struct
import zlib

RECORD_SIZE = 158
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3


def u32_le(data, at=0):
    return struct.unpack_from('<I', data, at)[0]


def fnv64(data):
    value = FNV_OFFSET
    for byte in data:
        value = (value ^ byte) * FNV_PRIME & 0xFFFFFFFFFFFFFFFF
    return value


def package_entries(blob):
    active = set()

    def walk(offset, prefix=''):
        if offset in active or offset + 12 > len(blob):
            raise ValueError('invalid or cyclic package directory')
        active.add(offset)
        size, count, record_size = struct.unpack_from('<III', blob, offset)
        if record_size != RECORD_SIZE or size < 12 or offset + size > len(blob):
            raise ValueError('invalid package directory header')
        for index in range(count):
            at = offset + 12 + index * RECORD_SIZE
            if at + RECORD_SIZE > len(blob):
                raise ValueError('truncated package directory')
            _, byte_count = struct.unpack_from('<II', blob, at)
            name = blob[at + 71:at + 134].split(b'\0', 1)[0].decode('cp1251', 'strict')
            kind = u32_le(blob, at + 134)
            flags = u32_le(blob, at + 142)
            target = u32_le(blob, at + 150)
            if flags:
                continue
            path = f'{prefix}/{name}' if prefix else name
            if kind == 3:
                yield from walk(target, path)
            else:
                yield path, kind, byte_count, target
        active.remove(offset)

    yield from walk(u32_le(blob))


def package_payload(blob, entry):
    _, kind, byte_count, target = entry
    at = target + 4
    if kind != 2:
        if at + byte_count > len(blob):
            raise ValueError('truncated raw package payload')
        return blob[at:at + byte_count]
    out = bytearray()
    remaining = byte_count
    while remaining:
        if at + 8 > len(blob):
            raise ValueError('truncated compressed package payload')
        packed = u32_le(blob, at)
        if packed < 8 or at + packed > len(blob):
            raise ValueError('invalid compressed package block')
        block = blob[at + 4:at + packed]
        if block[:4] != b'ZL02':
            raise ValueError('unsupported compressed package block')
        expected = min(remaining, 65536)
        if u32_le(block, 4) != expected:
            raise ValueError('invalid compressed package block length')
        decoded = zlib.decompress(block[8:])
        if len(decoded) != expected:
            raise ValueError('package block length mismatch')
        out.extend(decoded)
        remaining -= expected
        at += packed
    return bytes(out)


def paeth(left, up, up_left):
    p = left + up - up_left
    pa, pb, pc = abs(p - left), abs(p - up), abs(p - up_left)
    return left if pa <= pb and pa <= pc else up if pb <= pc else up_left


def decode_indexed_png(source):
    if source[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('not a PNG')
    at, width, height, bit_depth, color_type = 8, 0, 0, 0, 0
    palette, transparent, idat = None, b'', bytearray()
    while at + 12 <= len(source):
        size = struct.unpack_from('>I', source, at)[0]
        kind = source[at + 4:at + 8]
        payload_end = at + 8 + size
        if payload_end + 4 > len(source):
            raise ValueError('truncated PNG chunk')
        payload = source[at + 8:payload_end]
        if zlib.crc32(kind + payload) & 0xFFFFFFFF != struct.unpack_from('>I', source, payload_end)[0]:
            raise ValueError('PNG chunk CRC mismatch')
        if kind == b'IHDR':
            width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack('>IIBBBBB', payload)
            if not width or not height or bit_depth != 8 or color_type != 3 or compression or filtering or interlace not in (0, 1):
                raise ValueError('expected indexed 8-bit PNG')
        elif kind == b'PLTE':
            if len(payload) % 3 or not payload:
                raise ValueError('invalid PNG palette')
            palette = [tuple(payload[index:index + 3]) for index in range(0, len(payload), 3)]
        elif kind == b'tRNS':
            transparent = payload
        elif kind == b'IDAT':
            idat.extend(payload)
        elif kind == b'IEND':
            break
        at = payload_end + 4
    if not palette or not idat:
        raise ValueError('PNG palette or data is missing')
    raw = zlib.decompress(idat)
    cursor = 0
    pixels_by_index = bytearray(width * height)

    def decode_pass(start_x, start_y, step_x, step_y):
        nonlocal cursor
        pass_width = (width - start_x + step_x - 1) // step_x if width > start_x else 0
        pass_height = (height - start_y + step_y - 1) // step_y if height > start_y else 0
        if not pass_width or not pass_height:
            return
        previous = bytearray(pass_width)
        for row_index in range(pass_height):
            if cursor + 1 + pass_width > len(raw):
                raise ValueError('truncated indexed PNG scanline')
            filter_type = raw[cursor]
            cursor += 1
            row = bytearray(raw[cursor:cursor + pass_width])
            cursor += pass_width
            for index, value in enumerate(row):
                left = row[index - 1] if index else 0
                up = previous[index]
                up_left = previous[index - 1] if index else 0
                if filter_type == 1:
                    row[index] = (value + left) & 0xff
                elif filter_type == 2:
                    row[index] = (value + up) & 0xff
                elif filter_type == 3:
                    row[index] = (value + ((left + up) >> 1)) & 0xff
                elif filter_type == 4:
                    row[index] = (value + paeth(left, up, up_left)) & 0xff
                elif filter_type != 0:
                    raise ValueError('unsupported PNG filter')
            for column_index, value in enumerate(row):
                pixels_by_index[(start_y + row_index * step_y) * width + start_x + column_index * step_x] = value
            previous = row

    if interlace == 0:
        decode_pass(0, 0, 1, 1)
    else:
        for pass_spec in ((0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8),
                          (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2),
                          (0, 1, 1, 2)):
            decode_pass(*pass_spec)
    if cursor != len(raw):
        raise ValueError('unexpected indexed PNG scanline size')

    rgb565 = bytearray()
    for index in pixels_by_index:
        if index >= len(palette):
            raise ValueError('PNG palette index out of range')
        red, green, blue = palette[index]
        # DecodeNative565 has no alpha mask: tRNS does not alter this path.
        word = (red >> 3) << 11 | (green >> 2) << 5 | (blue >> 3)
        rgb565.extend(struct.pack('<H', word))
    return width, height, bytes(rgb565), len(transparent)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('game_root', type=pathlib.Path)
    parser.add_argument('--json', type=pathlib.Path)
    args = parser.parse_args()
    package = args.game_root / 'DATA' / 'common.pkg'
    resource = 'DATA/Planet/Spu00.png'
    blob = package.read_bytes()
    entry = next((item for item in package_entries(blob) if item[0].casefold() == resource.casefold()), None)
    if entry is None:
        raise SystemExit(f'configured baseline is absent: {resource}')
    source = package_payload(blob, entry)
    width, height, pixels, trns_count = decode_indexed_png(source)
    result = {
        'status': 'PRESENT', 'mode': 'Simple', 'resource': resource,
        'package': package.name, 'source_size': len(source),
        'source_crc32': f'{zlib.crc32(source) & 0xffffffff:08x}',
        'source_fnv64': f'{fnv64(source):016x}',
        'decoded_format': 'RGB565', 'width': width, 'height': height,
        'pitch': width * 2, 'decoded_bytes': len(pixels),
        'decoded_crc32': f'{zlib.crc32(pixels) & 0xffffffff:08x}',
        'decoded_fnv64': f'{fnv64(pixels):016x}',
        'trns_entries': trns_count,
    }
    text = json.dumps(result, ensure_ascii=False, indent=2)
    print(text)
    if args.json:
        args.json.write_text(text + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
