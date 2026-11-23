"""Rebuild only the verified Gov object in a separate Switch SDK archive."""
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path
from matrixgame_gov_autosave_guard import apply
from matrixgame_campaign_exception_overrides import apply as apply_exception
from matrixgame_legacy_wav_sdk_dependency import members

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 'build/matrixgame/gov-autosave-sdk-2'
old = ROOT / 'build/matrixgame/snapshot-prep-sdk-1/liboriginal-rangers-host-snapshotprep.a'
if out.exists(): raise RuntimeError('output already exists')
out.mkdir()
env = dict(os.environ, DEVKITPRO='C:/devkitPro', DEVKITA64='C:/devkitPro/devkitA64')
result = {'status': 'INCOMPLETE', 'commands': []}
def sha(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()
def run(label, command):
    command = list(map(str, command))
    p = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf8', errors='replace')
    (out / (label + '.log')).write_text(p.stdout + p.stderr, encoding='utf8')
    result['commands'].append({'label': label, 'exit': p.returncode, 'command': command})
    print(label, p.returncode, flush=True)
    if p.returncode: raise RuntimeError('see ' + label + '.log')
try:
    flags = json.loads((ROOT / 'port/switch/build/e2e-game-perf515/compile-flags.txt').read_text(encoding='utf8'))
    original = ROOT / 'port/switch/build/e2e-game-perf515/generated/fGov.cpp'
    source = out / 'fGov.cpp'; text = apply_exception('fGov.cpp', original.read_text(encoding='utf8')); source.write_text(text, encoding='utf8')
    raw = old.read_bytes(); assert raw[:8] == b'!<arch>\n'
    offset = 8; names = b''; payload = None
    while offset < len(raw):
        entry = raw[offset:offset+60]; assert entry[58:] == b'`\n'
        name = entry[:16].decode('ascii').strip(); size = int(entry[48:58]); body = raw[offset+60:offset+60+size]
        if name == '//': names = body
        elif name not in ('/', '/SYM64/'):
            if name.startswith('/'):
                start = int(name[1:]); name = names[start:names.index(b'/\n', start)].decode()
            else: name = name.rstrip('/')
            if name == 'fGov.o': assert payload is None; payload = body
        offset += 60 + size + size % 2
    assert payload is not None
    old_obj = out / 'original-fGov.o'; old_obj.write_bytes(payload); assert sha(old_obj) == members(old)['fGov.o']
    baseline = out / 'baseline-fGov.o'
    run('compile-baseline', [flags['compiler'], *flags['flags'], '-c', source, '-o', baseline])
    objcopy = 'C:/devkitPro/devkitA64/bin/aarch64-none-elf-objcopy.exe'
    for name, obj in (('old', old_obj), ('baseline', baseline)):
        run('strip-' + name, [objcopy, '--strip-debug', obj, out / (name + '-stripped.o')])
    assert sha(out / 'old-stripped.o') == sha(out / 'baseline-stripped.o')
    source.write_text(apply('fGov.cpp', text), encoding='utf8')
    obj = out / 'fGov.o'; run('compile-fixed', [flags['compiler'], *flags['flags'], '-c', source, '-o', obj])
    archive = out / 'liboriginal-rangers-host-govsave.a'; old_hash = sha(old); shutil.copyfile(old, archive)
    run('replace-member', ['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe', 'rs', archive, obj])
    before, after = members(old), members(archive); assert set(before) == set(after)
    changed = sorted(name for name in before if before[name] != after[name])
    assert changed == ['fGov.o'] and after['fGov.o'] == sha(obj) and sha(old) == old_hash
    result.update(status='PASS; verified SDK Gov object rebuilt; hardware NOT RUN', archive=str(archive), archive_sha256=sha(archive), old_archive_sha256=old_hash, changed_members=changed, generated_source_sha256=sha(source), overlay_sha256=sha(ROOT / 'tools/matrixgame_gov_autosave_guard.py'))
finally:
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
