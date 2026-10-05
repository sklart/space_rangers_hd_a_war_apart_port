"""Host reference for the release chained-zlib package entry."""
import pathlib
import struct
import sys
import zlib

BLOCK = 65536

def entry_at(data, folder, name):
    _, count, record = struct.unpack_from("<III", data, folder)
    assert record == 158
    for index in range(count):
        offset = folder + 12 + index * record
        upper = data[offset + 8:offset + 71].split(b"\0", 1)[0].decode("ascii")
        if upper == name.upper():
            return offset
    raise AssertionError(name)

def main(root):
    data = pathlib.Path(root, "DATA", "common.pkg").read_bytes()
    root_folder, = struct.unpack_from("<I", data)
    data_entry = entry_at(data, root_folder, "DATA")
    data_folder, = struct.unpack_from("<I", data, data_entry + 150)
    asteroid_entry = entry_at(data, data_folder, "ASTEROID")
    asteroid_folder, = struct.unpack_from("<I", data, asteroid_entry + 150)
    entry = entry_at(data, asteroid_folder, "00.GAI")
    stored, size = struct.unpack_from("<II", data, entry)
    kind, = struct.unpack_from("<i", data, entry + 134)
    position, = struct.unpack_from("<I", data, entry + 150)
    assert kind == 2 and size == 246863 and stored > 0
    position += 4
    output = bytearray()
    while len(output) < size:
        packed_size, = struct.unpack_from("<I", data, position)
        position += 4
        block = data[position:position + packed_size]
        assert block[:4] == b"ZL02"
        output.extend(zlib.decompress(block[8:]))
        position += packed_size
    output = output[:size]
    assert len(output) == size
    print(f"path=DATA/Asteroid/00.gai kind={kind} size={size} crc32={zlib.crc32(output):08x}")

if __name__ == "__main__":
    main(sys.argv[1])
