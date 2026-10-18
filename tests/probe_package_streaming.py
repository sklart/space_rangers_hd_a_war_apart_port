"""Bounded streaming of a >1 MiB package, including cross-block seeks/corruption."""
from pathlib import Path
import argparse
import struct
import subprocess
import zlib

root = Path(__file__).resolve().parents[1]
out = root / 'build/package-streaming'
out.mkdir(parents=True, exist_ok=True)
data = bytes((i * 17) % 251 for i in range(1100003))
payload = bytearray()
for i in range(0, len(data), 65536):
    block = data[i:i + 65536]
    packed = b'ZL02' + struct.pack('<I', len(block)) + zlib.compress(block, 0)
    payload += struct.pack('<I', len(packed)) + packed
record = struct.pack('<II63s63siiIIII', len(payload), len(data), b'MUSIC', b'MUSIC', 2, 2, 0, 0, 174, 0)
fixture = struct.pack('<IIII', 4, 170, 1, 158) + record + bytes(4) + payload
(out / 'fixture.pkg').write_bytes(fixture)
corrupt = bytearray(fixture)
corrupt[-1] ^= 0xff
(out / 'corrupt.pkg').write_bytes(corrupt)
raw_record = struct.pack('<II63s63siiIIII', len(data), len(data), b'MUSIC', b'MUSIC', 1, 1, 0, 0, 174, 0)
(out / 'raw.pkg').write_bytes(struct.pack('<IIII', 4, 170, 1, 158) + raw_record + bytes(4) + data)

# A separate large fixture keeps the 17-block late-corruption fixture stable
# for the real TFileStreamEC integration test.
large_data = bytes((i * 17) % 251 for i in range(259 * 65536 + 17))
large_payload = bytearray()
for i in range(0, len(large_data), 65536):
    block = large_data[i:i + 65536]
    packed = b'ZL02' + struct.pack('<I', len(block)) + zlib.compress(block, 0)
    large_payload += struct.pack('<I', len(packed)) + packed
large_record = struct.pack('<II63s63siiIIII', len(large_payload), len(large_data), b'MUSIC', b'MUSIC', 2, 2, 0, 0, 174, 0)
(out / 'large.pkg').write_bytes(struct.pack('<IIII', 4, 170, 1, 158) + large_record + bytes(4) + large_payload)
subprocess.run(['g++', '-std=c++20', '-O1', '-g', '-fsanitize=address,undefined',
                '-fno-omit-frame-pointer', '-DE2E511_PACKAGE_IO_PROFILE=1',
                '-Wl,--wrap=fseeko,--wrap=fread,--wrap=uncompress', '-Iport/switch/platform',
                'tests/test_package_streaming.cpp', 'port/switch/platform/package.cpp',
                '-lz', '-o', str(out / 'probe')], cwd=root, check=True)
subprocess.run([str(out / 'probe'), str(out / 'fixture.pkg'), str(out / 'corrupt.pkg'), str(out / 'large.pkg'), str(out / 'raw.pkg')], check=True)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--game-root', type=Path)
args = parser.parse_args()
if args.game_root:
    for name in ('music.pkg', 'forms.pkg'):
        subprocess.run([str(out / 'probe'), str(args.game_root / 'DATA' / name)], check=True)
