"""Link a new isolated Switch test ELF against the validated save archive."""
import hashlib
import json
import os
import subprocess
from pathlib import Path

from matrixgame_windows_host_env import CMAKE

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/matrixgame/integrated-app-20261123'
OUT = ROOT / 'build/matrixgame/integrated-e2e516-gpu-image-20261124-1'
OLD = BUILD / 'matrixgame-integrated-app-e2e516-gpu-depth.elf'
NAME = 'matrixgame-integrated-app-e2e516-gpu-image'
if OUT.exists():
    raise RuntimeError('evidence output already exists')
OUT.mkdir(parents=True)
result = {'status': 'INCOMPLETE', 'commands': []}

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def run(label, command):
    command = list(map(str, command))
    process = subprocess.run(command, cwd=ROOT, env=dict(os.environ, DEVKITPRO='C:/devkitPro'),
                             capture_output=True, text=True, encoding='utf-8', errors='replace')
    (OUT / (label + '.log')).write_text(process.stdout + process.stderr, encoding='utf-8')
    result['commands'].append({'label': label, 'command': command, 'exit': process.returncode})
    print(label, process.returncode, flush=True)
    if process.returncode:
        raise RuntimeError('see ' + label + '.log')

try:
    prior_hash = sha(OLD)
    baseline = json.loads((ROOT / 'dist/integrated-e2e516-test-20261124-19/manifest.json').read_text(encoding='utf-8'))
    assert prior_hash == baseline['elf_sha256']
    sdk = json.loads((ROOT / 'build/matrixgame/e2e516-detail-perf-sdk-20261124-1/result.json').read_text(encoding='utf-8'))
    archive = Path(sdk['archive'])
    assert sdk['status'].startswith('PASS') and sha(archive) == sdk['archive_sha256']
    elf = BUILD / (NAME + '.elf')
    assert not elf.exists()
    run('configure', [CMAKE, '-S', ROOT / 'port/matrixgame/native-engine', '-B', BUILD,
                      '-DMATRIXGAME_INTEGRATED_E2E516_STARTUP=ON', '-DMATRIXGAME_INTEGRATED_OUTPUT_NAME=' + NAME,
                      '-DBRIDGE_RANGERS_HOST_ARCHIVE=' + str(archive)])
    run('build', [CMAKE, '--build', BUILD, '--target', 'matrixgame-integrated-app', '-j', '4'])
    assert elf.is_file() and sha(OLD) == prior_hash
    result.update(status='PASS; ARM64 SDK compile/link; hardware NOT RUN', elf=str(elf), elf_sha256=sha(elf),
                  prior_elf_sha256=prior_hash, rangers_archive=str(archive), rangers_archive_sha256=sha(archive))
finally:
    (OUT / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
