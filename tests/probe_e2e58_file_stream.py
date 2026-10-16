"""ASan/UBSan real TFileStreamEC integration; requires the optimized host build."""
from pathlib import Path
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

root = Path.cwd()
build = root / 'port/switch/build/e2e5-host-game/optimized'
out = root / 'build/e2e58/file-stream'
out.mkdir(parents=True, exist_ok=True)
# A deliberately large compressed multi-block resource; not a game asset.
data = bytes((i * 17) % 251 for i in range(1100003))
payload = b''
for i in range(0, len(data), 65536):
    block = data[i:i + 65536]
    packed = b'ZL02' + struct.pack('<I', len(block)) + zlib.compress(block, 0)
    payload += struct.pack('<I', len(packed)) + packed
# Level 0 full blocks allocate 65560 bytes including the package block header.
record = struct.pack('<II63s63siiIIII', len(payload), len(data), b'MUSIC',
                     b'MUSIC', 2, 2, 0, 0, 174, 0)
(out / 'fixture.pkg').write_bytes(struct.pack('<IIII', 4, 170, 1, 158) +
                                record + bytes(4) + payload)
corrupt = bytearray((out / 'fixture.pkg').read_bytes())
# Last block fails after sixteen successful decompressions.
corrupt[-1] ^= 0xff
(out / 'corrupt.pkg').write_bytes(corrupt)
flags = json.loads((build / 'report.json').read_text())['compile_flags']
flags = [f for f in flags if not f.startswith(('-O', '-g'))]
flags += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
sys.path.insert(0, str(root / 'tools'))
from e2e_source_overrides import generated_source
objects = []
replace = {'EC_File', 'EC_FileStream', 'EC_Thread', 'package', 'ec_file_adapter',
           'e2e5_host_game_main'}
with tempfile.TemporaryDirectory(prefix='e2e58-real-stream-') as temp:
    staging = Path(temp)
    for original in (build / 'link-objects.rsp').read_text().splitlines():
        obj = Path(original)
        if obj.stem in replace:
            continue
        target = staging / f'{len(objects)}.o'
        shutil.copyfile(obj, target)
        objects.append(target)
    for name in sorted(replace - {'e2e5_host_game_main'}):
        if name in {'package', 'ec_file_adapter'}:
            source = root / 'port/switch/platform' / (name + '.cpp')
        else:
            source = generated_source(root / 'upstream/cpp/src' / (name + '.cpp'),
                                      out / (name + '.cpp'), audio_on=True, e2e5_profile=True)
        target = staging / (name + '.o')
        subprocess.run(['g++', *flags, '-c', str(source), '-o', str(target)], check=True)
        objects.append(target)
    response = staging / 'objects.rsp'
    response.write_text('\n'.join(map(str, objects)))
    library = build / 'okgf/libokgf.so'
    binary = out / 'probe'
    subprocess.run(['g++', *flags, 'tests/test_e2e58_file_stream.cpp', '@' + str(response),
                    str(library), '-Wl,--gc-sections', '-Wl,-rpath,' + str(library.parent),
                    '-lz', '-lpng', '-ljpeg', '-lvorbisfile', '-lvorbis', '-logg',
                    '-lSDL2', '-pthread', '-o', str(binary)], check=True)
subprocess.run([str(binary), str(out / 'fixture.pkg'), str(out / 'corrupt.pkg')], check=True, timeout=20)
