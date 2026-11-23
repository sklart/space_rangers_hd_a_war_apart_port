"""Rebuild the isolated Switch TEST ELF without replacing prior outputs."""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path
from matrixgame_windows_host_env import CMAKE

ROOT = Path(__file__).resolve().parents[1]
build = ROOT / 'build/matrixgame/integrated-app-20261123'
battlecommit = sys.argv[1:] == ['--battlecommit']
govsave = sys.argv[1:] == ['--govsave'] or battlecommit
snapshot = sys.argv[1:] == ['--snapshot'] or govsave
buffers = sys.argv[1:] == ['--buffers'] or snapshot
ship = sys.argv[1:] == ['--ship'] or buffers
if sys.argv[1:] and not ship:
    raise RuntimeError('unknown arguments')
out = ROOT / ('build/matrixgame/integrated-battlecommit-20261123-1' if battlecommit else 'build/matrixgame/integrated-govsave-20261123-1' if govsave else 'build/matrixgame/integrated-snapshotprep-20261123-2' if snapshot else 'build/matrixgame/integrated-bufferfix-20261123-1' if buffers else 'build/matrixgame/integrated-savefix-20261123-1' if ship else 'build/matrixgame/integrated-exceptionfix-20261123-2')
if out.exists():
    raise RuntimeError('build evidence directory already exists')
out.mkdir()
env = dict(os.environ, DEVKITPRO='C:/devkitPro')
result = {'status': 'INCOMPLETE', 'commands': []}

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def run(label, command):
    command = list(map(str, command))
    process = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, encoding='utf8', errors='replace')
    (out / (label + '.log')).write_text(process.stdout + process.stderr, encoding='utf8')
    result['commands'].append({'label': label, 'exit': process.returncode, 'command': command})
    print(label, process.returncode, flush=True)
    if process.returncode:
        raise RuntimeError('see ' + label + '.log')

try:
    old = build / ('matrixgame-integrated-app-govsave.elf' if battlecommit else 'matrixgame-integrated-app-snapshotprep.elf' if govsave else 'matrixgame-integrated-app-bufferfix.elf' if snapshot else 'matrixgame-integrated-app-savefix.elf' if buffers else 'matrixgame-integrated-app-shipfix.elf' if ship else 'matrixgame-integrated-app-logtest.elf')
    old_hash = sha(old)
    name = 'matrixgame-integrated-app-battlecommit' if battlecommit else 'matrixgame-integrated-app-govsave' if govsave else 'matrixgame-integrated-app-snapshotprep' if snapshot else 'matrixgame-integrated-app-bufferfix' if buffers else 'matrixgame-integrated-app-savefix' if ship else 'matrixgame-integrated-app-exceptionfix'
    configure = [CMAKE, '-S', ROOT / 'port/matrixgame/native-engine', '-B', build,
                 '-DMATRIXGAME_INTEGRATED_OUTPUT_NAME=' + name]
    if ship:
        sdk_result = json.loads((ROOT / ('build/matrixgame/battle-save-commit-sdk-2/result.json' if battlecommit else 'build/matrixgame/gov-autosave-sdk-2/result.json' if govsave else 'build/matrixgame/snapshot-prep-sdk-1/result.json' if snapshot else 'build/matrixgame/save-buffer-sdk-1/result.json' if buffers else 'build/matrixgame/ship-reference-sdk-3/result.json')).read_text(encoding='utf8'))
        archive = Path(sdk_result['archive'])
        assert sdk_result['status'].startswith('PASS') and sha(archive) == sdk_result['archive_sha256']
        configure.append('-DBRIDGE_RANGERS_HOST_ARCHIVE=' + str(archive))
        result['rangers_archive'] = str(archive)
        result['rangers_archive_sha256'] = sha(archive)
    run('configure', configure)
    run('build', [CMAKE, '--build', build, '--target', 'matrixgame-integrated-app', '-j', '4'])
    elf = build / (name + '.elf')
    assert elf.is_file() and sha(old) == old_hash
    result.update(status='PASS; ARM64 SDK compile/link; hardware NOT RUN', elf=str(elf), elf_sha256=sha(elf),
                  prior_elf_sha256=old_hash, source_sha256=sha(ROOT / 'port/matrixgame/native-engine/native_rangers_interface.cpp'))
finally:
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
