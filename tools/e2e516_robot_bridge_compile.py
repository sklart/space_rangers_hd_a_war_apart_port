"""Compile an isolated E2E516 Robot unit with the existing battle handoff patches.

This does not link a game, modify the frozen baseline, or claim runtime support.
"""
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path

from matrixgame_campaign_exception_overrides import apply as exception_recovery
from matrixgame_campaign_caller_overrides import apply as caller_handoff

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'port/switch/build/e2e-game-perf515'
ZIP = ROOT / 'build/e2e516/hardware-e2e516-03651ce.zip'
OUT = ROOT / 'build/matrixgame/e2e516-robot-bridge-20261124-1'

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    if OUT.exists():
        raise RuntimeError('Output exists; previous evidence must be retained')
    with zipfile.ZipFile(ZIP) as z:
        inputs = json.loads(z.read('source-sha256.json'))
        mapping = json.loads(z.read('effective-source-map.json'))
    relative = mapping['upstream/cpp/src/Robot.cpp']
    source = ROOT / relative
    if digest(source) != inputs[relative]:
        raise RuntimeError('Effective Robot source does not match frozen E2E516')
    original = source.read_text(encoding='utf-8')
    patched = caller_handoff('Robot.cpp', exception_recovery('Robot.cpp', original))
    for marker in ('planet_battle::Capture()', 'native_caller->EnterBattle()',
                   'native_caller->Restore()', 'native_run_error'):
        if marker not in patched:
            raise RuntimeError('Missing bridge boundary: ' + marker)
    flags = json.loads((BASE / 'compile-flags.txt').read_text(encoding='utf-8'))
    OUT.mkdir(parents=True)
    target = OUT / 'Robot.cpp'
    target.write_text(patched, encoding='utf-8', newline='\n')
    obj = OUT / 'Robot.o'
    command = [flags['compiler'], *flags['flags'], '-c', str(target), '-o', str(obj)]
    process = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    (OUT / 'compile.log').write_text(process.stdout + process.stderr, encoding='utf-8')
    result = {'status': 'COMPILE FAILED' if process.returncode else 'SDK OBJECT COMPILED; LINK AND SWITCH NOT RUN',
              'source': relative, 'baseline_source_sha256': digest(source),
              'generated_source_sha256': digest(target), 'command': command, 'exit': process.returncode}
    if obj.exists():
        result['object_sha256'] = digest(obj)
    (OUT / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(result['status'])
    if process.returncode:
        raise RuntimeError('See compile.log')

if __name__ == '__main__':
    main()
