"""Reload copies of the two dispatcher autosaves in separate processes."""
import hashlib
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    out = BASE / 'gov-frun-reload-runtime-2'
    if out.exists():
        raise RuntimeError('output already exists')
    out.mkdir()
    result = {'status': 'INCOMPLETE', 'runs': [], 'scope': 'two isolated Windows save reloads; Switch NOT RUN'}
    try:
        previous = BASE / 'gov-frun-dispatcher-runtime-1'
        assert json.loads((previous / 'result.json').read_text(encoding='utf8'))['status'].startswith('PASS')
        log = (previous / 'runtime.log').read_text(encoding='utf8', errors='replace')
        states = list(dict.fromkeys(re.findall(r'^E2E5_STATE\|save\|([0-9|]+)$', log, re.M)))
        assert len(states) == 2, states
        save_dir = previous / 'output/rangers-user/config/save'
        sources = [save_dir / 'AutoSave.sav.bak', save_dir / 'AutoSave.sav']
        source_hashes = [sha(path) for path in sources]
        binary = BASE / 'gov-frun-reload-link-2/gov-frun-reload.exe'
        fixture = BASE / 'gov-frun-runtime-1/native-input'
        env = dict(os.environ, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='windows')
        env['PATH'] = str(BASE / 'deps/sdl2/SDL2-2.30.11/lib/x64') + ';' + str(BASE / 'deps/d3dx/build/native/release/bin/x64') + ';' + env.get('PATH', '')
        for index, (state, source) in enumerate(zip(states, sources)):
            run = out / ('backup' if index == 0 else 'current')
            output = run / 'output'
            destination = output / 'rangers-user/config/save/AutoSave.sav'
            destination.parent.mkdir(parents=True)
            shutil.copyfile(source, destination)
            assert sha(destination) == source_hashes[index]
            run_env = dict(env, SRHD_TEST_EXPECTED_STATE=state.replace('|', ',') + ',' + str(index))
            command = [str(binary), str(output), str(ROOT / 'windows/Space Rangers HD A War Apart'), str(fixture)]
            print('reload', index + 1, 'started', flush=True)
            with (run / 'runtime.log').open('w', encoding='utf8') as stream:
                process = subprocess.run(command, cwd=run, env=run_env, stdout=stream, stderr=subprocess.STDOUT)
            item = {'source': str(source), 'source_sha256': source_hashes[index], 'expected': run_env['SRHD_TEST_EXPECTED_STATE'], 'exit': process.returncode}
            result['runs'].append(item)
            if process.returncode:
                raise RuntimeError('reload failed; see ' + str(run / 'runtime.log'))
            text = (run / 'runtime.log').read_text(encoding='utf8', errors='replace')
            assert 'AUTOSAVE_RELOAD original_load=PASS' in text
            assert 'original_campaign_UI_script_host_cleanup=PASS' in text
            assert sha(source) == source_hashes[index]
            print('reload', index + 1, 'passed', flush=True)
        result['status'] = 'PASS; backup and current autosaves reloaded in separate Windows processes; Switch NOT RUN'
    finally:
        (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')

if __name__ == '__main__':
    main()
