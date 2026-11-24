"""Compile the modified E2E516 renderer privately; do not link or package."""
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'port/switch/build/e2e-game-perf515'
OUT = ROOT / 'build/matrixgame/e2e516-sdl-handoff-20261124-1'

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    if OUT.exists():
        raise RuntimeError('Output exists; preserve prior evidence')
    flags = json.loads((BASE / 'compile-flags.txt').read_text(encoding='utf-8'))
    source = ROOT / 'port/switch/platform/renderer_platform.cpp'
    headers = [ROOT / 'port/switch/platform' / name for name in
               ('renderer_platform.hpp', 'sdl_presentation_handoff.hpp', 'rgb565_presentation.hpp')]
    OUT.mkdir(parents=True)
    obj = OUT / 'renderer_platform.o'
    command = [flags['compiler'], *flags['flags'], '-c', str(source), '-o', str(obj)]
    process = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
    (OUT / 'compile.log').write_text(process.stdout + process.stderr, encoding='utf-8')
    result = {'status': 'SDK OBJECT COMPILED; LINK AND SWITCH NOT RUN' if not process.returncode else 'COMPILE FAILED',
              'sources_sha256': {str(p.relative_to(ROOT)): sha(p) for p in [source, *headers]},
              'command': command, 'exit': process.returncode}
    if obj.exists():
        result['object_sha256'] = sha(obj)
    (OUT / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(result['status'])
    if process.returncode:
        raise RuntimeError('See compile.log')

if __name__ == '__main__':
    main()
