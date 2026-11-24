"""Restore existing Switch GR_Main portability patches in an isolated archive."""
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path
from e2e_source_overrides import generated_source
from matrixgame_legacy_wav_sdk_dependency import members

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/matrixgame/integrated-grmain-sdk-20261124-1'
OLD = ROOT / 'build/matrixgame/save-queue-sdk-20261123-1/liboriginal-rangers-host-savequeue.a'
PATCH_ROOT = ROOT / 'port/switch/build/e2e-game-perf515/patched-upstream'
ORIGINAL_BUILD = ROOT / 'build/matrixgame/e2e610-native-ui-pump/sdk/6'
if OUT.exists():
    raise RuntimeError('output already exists')
OUT.mkdir(parents=True)
result = {'status': 'INCOMPLETE', 'commands': [], 'hardware': 'NOT RUN'}

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def run(label, command):
    command = list(map(str, command))
    env = dict(os.environ, DEVKITPRO='C:/devkitPro', DEVKITA64='C:/devkitPro/devkitA64')
    p = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf8', errors='replace')
    (OUT / (label + '.log')).write_text(p.stdout + p.stderr, encoding='utf8')
    result['commands'].append({'label': label, 'command': command, 'exit': p.returncode})
    print(label, p.returncode, flush=True)
    if p.returncode:
        raise RuntimeError('see ' + label + '.log')
    return p.stdout

try:
    prior = json.loads((ROOT / 'build/matrixgame/save-queue-sdk-20261123-1/result.json').read_text(encoding='utf8'))
    assert prior['status'].startswith('PASS') and sha(OLD) == prior['archive_sha256']
    stamp = json.loads((PATCH_ROOT / 'patch-inputs.json').read_text(encoding='utf8'))
    assert all(sha(ROOT / name) == value for name, value in stamp.items())
    old_members = members(OLD)
    assert old_members['GR_Main.o'] == sha(ORIGINAL_BUILD / 'GR_Main.o')
    old_build = json.loads((ORIGINAL_BUILD / 'result.json').read_text(encoding='utf8'))
    command = next(item['command'] for item in old_build['commands'] if item['label'] == 'compile-production-GR_Main')[:]
    old_source = ORIGINAL_BUILD / 'GR_Main.cpp'
    baseline = OUT / 'baseline-GR_Main.o'
    command[command.index('-c') + 1] = str(old_source)
    command[command.index('-o') + 1] = str(baseline)
    run('compile-baseline', command)
    objcopy = 'C:/devkitPro/devkitA64/bin/aarch64-none-elf-objcopy.exe'
    for label, obj in [('old', ORIGINAL_BUILD / 'GR_Main.o'), ('baseline', baseline)]:
        run('strip-' + label, [objcopy, '--strip-debug', obj, OUT / (label + '-stripped.o')])
    assert sha(OUT / 'old-stripped.o') == sha(OUT / 'baseline-stripped.o')
    source = OUT / 'GR_Main.cpp'
    generated_source(PATCH_ROOT / 'src/GR_Main.cpp', source, audio_on=True, e2e5_profile=True, script_width=True)
    text = source.read_text(encoding='utf8')
    assert 'rangers_ui_input::installed' in text and 'renderer_platform::PresentRgb565' in text
    obj = OUT / 'GR_Main.o'
    command[command.index('-c') + 1] = str(source)
    command[command.index('-o') + 1] = str(obj)
    run('compile-portable', command)
    nm = 'C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe'
    symbols = run('undefined-symbols', [nm, '-C', '--undefined-only', obj])
    assert 'renderer_platform::PresentRgb565' in symbols
    archive = OUT / 'liboriginal-rangers-host-portable-present.a'
    shutil.copyfile(OLD, archive)
    run('replace', ['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe', 'rs', archive, obj])
    new_members = members(archive)
    assert set(new_members) == set(old_members)
    changed = [name for name in old_members if old_members[name] != new_members[name]]
    assert changed == ['GR_Main.o'] and new_members['GR_Main.o'] == sha(obj)
    assert sha(OLD) == prior['archive_sha256']
    result.update(status='PASS; patched GR_Main SDK archive; hardware NOT RUN', archive=str(archive),
                  archive_sha256=sha(archive), old_archive_sha256=sha(OLD), changed_members=changed,
                  source_sha256=sha(source), original_source_sha256=sha(old_source),
                  patched_input_sha256=sha(PATCH_ROOT / 'src/GR_Main.cpp'), patch_inputs_sha256=stamp,
                  archive_members_sha256=new_members)
finally:
    (OUT / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
