"""Build one ownership-fixed saver object in a separate verified SDK archive."""
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from matrixgame_legacy_wav_sdk_dependency import members
from matrixgame_save_buffer_ownership import apply
from matrixgame_snapshot_preparation import apply as apply_snapshot

ROOT = Path(__file__).resolve().parents[1]
snapshot = sys.argv[1:] == ['--snapshot']
if sys.argv[1:] and not snapshot: raise RuntimeError('unknown arguments')
out = ROOT / ('build/matrixgame/snapshot-prep-sdk-1' if snapshot else 'build/matrixgame/save-buffer-sdk-1')
old = ROOT / 'build/matrixgame/ship-reference-sdk-3/liboriginal-rangers-host-shipfix.a'
if out.exists(): raise RuntimeError('output already exists')
out.mkdir()
env = dict(os.environ, DEVKITPRO='C:/devkitPro', DEVKITA64='C:/devkitPro/devkitA64')
result = {'status': 'INCOMPLETE', 'commands': []}

def sha(path):
    with path.open('rb') as stream: return hashlib.file_digest(stream, 'sha256').hexdigest()

def run(label, command):
    command = list(map(str, command))
    process = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf8', errors='replace')
    (out / (label + '.log')).write_text(process.stdout + process.stderr, encoding='utf8')
    result['commands'].append({'label': label, 'exit': process.returncode, 'command': command})
    print(label, process.returncode, flush=True)
    if process.returncode: raise RuntimeError('see ' + label + '.log')

try:
    flags = json.loads((ROOT / 'port/switch/build/e2e-game-perf515/compile-flags.txt').read_text(encoding='utf8'))
    original = ROOT / 'port/switch/build/e2e-game-perf515/generated/aSaveLoad.cpp'
    text = original.read_text(encoding='utf8')
    source = out / 'aSaveLoad.cpp'; source.write_text(text, encoding='utf8')
    raw = old.read_bytes(); assert raw[:8] == b'!<arch>\n'
    offset = 8; names = b''; payload = None
    while offset < len(raw):
        entry = raw[offset:offset + 60]; assert entry[58:] == b'`\n'
        name = entry[:16].decode('ascii').strip(); size = int(entry[48:58])
        body = raw[offset + 60:offset + 60 + size]
        if name == '//': names = body
        elif name not in ('/', '/SYM64/'):
            if name.startswith('/'):
                start = int(name[1:]); name = names[start:names.index(b'/\n', start)].decode()
            else: name = name.rstrip('/')
            if name == 'aSaveLoad.o': assert payload is None; payload = body
        offset += 60 + size + (size % 2)
    assert payload is not None
    old_obj = out / 'original-aSaveLoad.o'; old_obj.write_bytes(payload)
    assert sha(old_obj) == members(old)['aSaveLoad.o']
    baseline = out / 'baseline-aSaveLoad.o'
    run('compile-baseline', [flags['compiler'], *flags['flags'], '-c', source, '-o', baseline])
    objcopy = 'C:/devkitPro/devkitA64/bin/aarch64-none-elf-objcopy.exe'
    for name, obj in (('old', old_obj), ('baseline', baseline)):
        run('strip-' + name, [objcopy, '--strip-debug', obj, out / (name + '-stripped.o')])
    result['baseline_stripped_matches_old'] = sha(out / 'old-stripped.o') == sha(out / 'baseline-stripped.o')
    if not result['baseline_stripped_matches_old']:
        raise RuntimeError('reconstructed saver baseline differs from retained SDK member')
    fixed = apply(text)
    source.write_text(apply_snapshot(fixed) if snapshot else fixed, encoding='utf8')
    obj = out / 'aSaveLoad.o'
    run('compile-fixed', [flags['compiler'], *flags['flags'], '-c', source, '-o', obj])
    archive = out / ('liboriginal-rangers-host-snapshotprep.a' if snapshot else 'liboriginal-rangers-host-savebuffer.a'); old_hash = sha(old)
    shutil.copyfile(old, archive)
    run('replace-member', ['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe', 'rs', archive, obj])
    before, after = members(old), members(archive)
    assert set(before) == set(after)
    changed = sorted(name for name in before if before[name] != after[name])
    assert changed == ['aSaveLoad.o'] and after['aSaveLoad.o'] == sha(obj) and sha(old) == old_hash
    result.update(status='PASS; verified SDK saver object rebuilt; hardware NOT RUN', archive=str(archive),
                  archive_sha256=sha(archive), old_archive_sha256=old_hash, changed_members=changed,
                  generated_source_sha256=sha(source), original_source_sha256=sha(original),
                  overlay_sha256=sha(ROOT / 'tools/matrixgame_save_buffer_ownership.py'))
finally:
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
