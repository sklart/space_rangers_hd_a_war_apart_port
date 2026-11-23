"""Compile a focused Windows typed-ship save/load test against real game objects."""
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from matrixgame_windows_host_env import environment, compiler

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'
items = sys.argv[1:] in (['--items'], ['--items-fixed'])
fixed = sys.argv[1:] in (['--fixed'], ['--items-fixed'])
if sys.argv[1:] and not (fixed or items):
    raise RuntimeError('unknown arguments')
out = BASE / ('item-roots-fixed-1' if items and fixed else 'item-roots-baseline-1' if items else 'ship-reference-fixed-2' if fixed else 'ship-reference-baseline-1')
if out.exists():
    raise RuntimeError('output already exists')
out.mkdir()
env = environment(out)
result = {'status': 'INCOMPLETE', 'commands': []}

def run(label, command, *, expected=0, cwd=ROOT):
    command = list(map(str, command))
    process = subprocess.run(command, cwd=cwd, env=env, capture_output=True, text=True, encoding='utf8', errors='replace')
    (out / (label + '.log')).write_text(process.stdout + process.stderr, encoding='utf8')
    result['commands'].append({'label': label, 'exit': process.returncode, 'command': command})
    print(label, process.returncode, flush=True)
    if process.returncode != expected:
        raise RuntimeError('unexpected exit; inspect ' + label + '.log')
    return process.stdout + process.stderr

try:
    previous = json.loads((BASE / 'rangers-windows-2/result.json').read_text(encoding='utf8'))
    flags = previous['flags']
    obj = out / 'ship-test.obj'
    test_source = ROOT / ('tests/test_e2e_item_roots.cpp' if items else 'tests/test_e2e_ship_references.cpp')
    run('compile-test', [compiler(env), *flags, '/c', test_source, '/Fo' + str(obj)])
    items = (BASE / 'rangers-host-probe/link.rsp').read_text(encoding='utf8').splitlines()
    old_probe = str(BASE / 'rangers-host-probe/probe.obj')
    assert sum(line.strip('"') == old_probe for line in items) == 1
    replacements = {old_probe: obj}
    if fixed:
        galaxy = out / 'aGalaxy-fixed.obj'
        run('compile-galaxy', [compiler(env), *flags, '/c', BASE / 'rangers-windows-2/source/aGalaxy.cpp', '/Fo' + str(galaxy)])
        old_galaxy = str(BASE / 'rangers-windows-2/objects/original/aGalaxy.obj')
        assert sum(line.strip('"') == old_galaxy for line in items) == 1
        replacements[old_galaxy] = galaxy
    rsp = out / 'link.rsp'
    rsp.write_text('\n'.join('"' + str(replacements[x.strip('"')]) + '"' if x.strip('"') in replacements else x for x in items), encoding='utf8')
    binary = out / 'ship-test.exe'
    run('link', [compiler(env), '/nologo', '/MT', '@' + str(rsp), '/Fe' + str(binary), '/link', '/INCREMENTAL:NO',
                 'oleaut32.lib', 'ole32.lib', 'user32.lib', 'gdi32.lib', 'version.lib', 'winmm.lib', 'setupapi.lib', 'imm32.lib', 'advapi32.lib', 'shell32.lib'])
    shutil.copyfile(BASE / 'deps/sdl2/SDL2-2.30.11/lib/x64/SDL2.dll', out / 'SDL2.dll')
    output = run('runtime', [binary], expected=0 if fixed else 1, cwd=out)
    assert ('typed_id_and_reload=PASS' if fixed else 'temporary_shop=REJECTED' if items else 'live_unbound_galaxy_ship=REJECTED') in output
    result['status'] = ('PASS; item ownership roots typed ID and original reload' if items and fixed else
                        'REPRODUCED; valid temporary-shop item rejected' if items else
                        'PASS; live galaxy ship typed ID and original reload' if fixed else
                        'REPRODUCED; valid unbound live ship rejected')
finally:
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
