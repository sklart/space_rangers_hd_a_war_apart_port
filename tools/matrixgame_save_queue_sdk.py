"""Rebuild the save unit in a new SDK archive after Windows validation."""
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

from e2e_source_overrides import generated_source
from matrixgame_legacy_wav_sdk_dependency import members

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/matrixgame/save-queue-sdk-20261123-1'
OLD = ROOT / 'build/matrixgame/battle-save-commit-sdk-2/liboriginal-rangers-host-battlesave.a'
OVERLAY = ROOT / 'build/matrixgame/snapshot-prep-overlay-audit/source-overlay/aSaveLoad.cpp'
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
    process = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf-8', errors='replace')
    (OUT / (label + '.log')).write_text(process.stdout + process.stderr, encoding='utf-8')
    result['commands'].append({'label': label, 'command': command, 'exit': process.returncode})
    print(label, process.returncode, flush=True)
    if process.returncode:
        raise RuntimeError('see ' + label + '.log')

try:
    validation = json.loads((ROOT / 'build/matrixgame/windows-reference-rebuild-20261123/gov-frun-concurrent-save-runtime-1/result.json').read_text(encoding='utf-8'))
    assert validation['status'].startswith('PASS')
    old_hash = sha(OLD)
    previous = json.loads((ROOT / 'build/matrixgame/battle-save-commit-sdk-2/result.json').read_text(encoding='utf-8'))
    assert old_hash == previous['archive_sha256']
    flags = json.loads((ROOT / 'port/switch/build/e2e-game-perf515/compile-flags.txt').read_text(encoding='utf-8'))
    source = OUT / 'aSaveLoad.cpp'
    generated_source(OVERLAY, source, audio_on=True, e2e5_profile=True, script_width=True)
    text = source.read_text(encoding='utf-8')
    assert text.count('e2e5_save_commit_wait::SaveCall save_call;') == 1
    obj = OUT / 'aSaveLoad.o'
    run('compile', [flags['compiler'], *flags['flags'], '-c', source, '-o', obj])
    archive = OUT / 'liboriginal-rangers-host-savequeue.a'
    before = members(OLD)
    shutil.copyfile(OLD, archive)
    run('replace', ['C:/devkitPro/devkitA64/bin/aarch64-none-elf-ar.exe', 'rs', archive, obj])
    after = members(archive)
    assert set(before) == set(after)
    changed = [name for name in before if before[name] != after[name]]
    assert changed == ['aSaveLoad.o'] and after['aSaveLoad.o'] == sha(obj) and sha(OLD) == old_hash
    result.update(status='PASS; one SDK host member rebuilt; hardware NOT RUN', archive=str(archive),
                  archive_sha256=sha(archive), old_archive_sha256=old_hash, changed_members=changed,
                  old_members_sha256=before, members_sha256=after,
                  source_sha256=sha(source), overlay_sha256=sha(OVERLAY),
                  guard_sha256=sha(ROOT / 'port/switch/platform/e2e5_save_commit_wait.hpp'),
                  transform_sha256=sha(ROOT / 'tools/matrixgame_save_commit_wait.py'))
finally:
    (OUT / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
