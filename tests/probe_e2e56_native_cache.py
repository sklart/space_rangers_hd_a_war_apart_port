"""Native EC_Cache integration; requires an already completed optimized host build."""
from pathlib import Path
import json
import argparse
import os
import hashlib
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--thread-sanitizer', action='store_true')
args = parser.parse_args()
root = Path.cwd()
build = root / 'port/switch/build/e2e5-host-game/optimized'
out = root / ('build/e2e56/native-cache-tsan' if args.thread_sanitizer else 'build/e2e56/native-cache')
out.mkdir(parents=True, exist_ok=True)
report = json.loads((build / 'report.json').read_text())
assert report['profile'] == 'optimized'
game_binary = Path(report['binary'])
assert hashlib.sha256(game_binary.read_bytes()).hexdigest() == report['binary_sha256']
sources = [root / report['source_map']['upstream/cpp/src/' + name + '.cpp']
           for name in ['EC_Cache', 'EC_CacheBuf', 'EC_Thread']]
source = sources[0]
assert game_binary.stat().st_mtime >= max(*(p.stat().st_mtime for p in sources),
    (root / 'port/switch/platform/e2e510_heap_reserve.hpp').stat().st_mtime,
    (root / 'port/switch/platform/e2e5_memory_recovery.hpp').stat().st_mtime), 'finish the host rebuild first'
flags = report['compile_flags']
flags = [f for f in flags if not f.startswith(('-O', '-g'))]
flags += ['-O1', '-g', '-fno-omit-frame-pointer']
flags += (['-fsanitize=thread', '-DE2E56_RECLAIM_STRESS=1'] if args.thread_sanitizer
          else ['-fsanitize=address,undefined'])
(out / 'proof.json').write_text(json.dumps({'status': 'INCOMPLETE', 'reason': 'probe attempt has not passed'}) + '\n')
with tempfile.TemporaryDirectory(prefix='e2e56-native-cache-') as temp:
    staging = Path(temp)
    objects = []
    for original in (build / 'link-objects.rsp').read_text().splitlines():
        obj = Path(original)
        if obj.stem in {*(p.stem for p in sources), 'e2e5_host_game_main'}:
            continue
        target = staging / f'{len(objects)}.o'
        shutil.copyfile(obj, target)
        objects.append(target)
    for source in sources:
        target = staging / (source.stem + '.o')
        subprocess.run(['g++', *flags, '-c', str(source), '-o', str(target)], check=True)
        objects.append(target)
    response = staging / 'objects.rsp'
    response.write_text('\n'.join(map(str, objects)))
    binary = out / 'probe'
    library = build / 'okgf/libokgf.so'
    subprocess.run(['g++', *flags, 'tests/test_e2e56_native_cache.cpp', '@' + str(response),
                    str(library), '-Wl,--gc-sections', '-Wl,-rpath,' + str(library.parent),
                    '-lz', '-lpng', '-ljpeg', '-lvorbisfile', '-lvorbis', '-logg',
                    '-lSDL2', '-pthread', '-o', str(binary)], check=True)
env = os.environ.copy()
if args.thread_sanitizer:
    env['TSAN_OPTIONS'] = env.get('TSAN_OPTIONS', '') + ':halt_on_error=1:exitcode=66'
subprocess.run([str(binary)], cwd=out, env=env, check=True, timeout=45)
(out / 'proof.json').write_text(json.dumps({
    'scope': 'REAL EC_CACHE WITH CONTROLLED PRODUCER; HOST; NOT SWITCH',
    'sanitizer': 'ThreadSanitizer' if args.thread_sanitizer else 'ASan/UBSan',
    'host_binary_sha256': report['binary_sha256'],
    'effective_source_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
    'memory_recovery_header_sha256': hashlib.sha256((root / 'port/switch/platform/e2e5_memory_recovery.hpp').read_bytes()).hexdigest(),
    'test_source_sha256': hashlib.sha256((root / 'tests/test_e2e56_native_cache.cpp').read_bytes()).hexdigest(),
    'probe_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
    'status': 'PASS'}, indent=2) + '\n')
