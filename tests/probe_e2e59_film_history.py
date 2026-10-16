"""Link the real effective history and film modules with ASan/UBSan."""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import sys

root = Path.cwd()
build = root / 'port/switch/build/e2e5-host-game/optimized'
out = root / 'build/e2e59/film-history'
out.mkdir(parents=True, exist_ok=True)
report = json.loads((build / 'report.json').read_text())
flags = [f for f in report['compile_flags'] if not f.startswith(('-O', '-g'))]
flags += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
width_case = '--width' in sys.argv
oracle = '--oracle' in sys.argv
replace = {'e2e5_host_game_main'} if width_case else {'aEFilm', 'fFilmFile', 'e2e5_host_game_main'}
with tempfile.TemporaryDirectory(prefix='e2e59-history-') as temp:
    staging = Path(temp)
    objects = []
    for line in (build / 'link-objects.rsp').read_text().splitlines():
        obj = Path(line)
        if obj.stem in replace:
            continue
        target = staging / f'{len(objects)}.o'
        shutil.copyfile(obj, target)
        objects.append(target)
    for name in sorted(replace - {'e2e5_host_game_main'}):
        source = root / report['source_map']['upstream/cpp/src/' + name + '.cpp']
        if oracle and name == 'aEFilm':
            source = build / 'source-overlay/aEFilm.cpp'
        target = staging / (name + '.o')
        subprocess.run(['g++', *flags, '-c', str(source), '-o', str(target)], check=True)
        objects.append(target)
    rsp = staging / 'objects.rsp'
    rsp.write_text('\n'.join(map(str, objects)))
    lib = build / 'okgf/libokgf.so'
    binary = out / ('native-width' if width_case else 'oracle' if oracle else 'probe')
    source = 'tests/test_e2e59_native_width.cpp' if width_case else 'tests/test_e2e59_film_history.cpp'
    subprocess.run(['g++', *flags, source, '@' + str(rsp),
                    str(lib), '-Wl,--gc-sections', '-Wl,-rpath,' + str(lib.parent),
                    '-lz', '-lpng', '-ljpeg', '-lvorbisfile', '-lvorbis', '-logg',
                    '-lSDL2', '-pthread', '-o', str(binary)], check=True)
subprocess.run([str(binary)], check=True, timeout=20)
