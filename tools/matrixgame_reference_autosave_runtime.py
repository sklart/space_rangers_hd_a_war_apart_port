"""Run the original Gov autosave handler and two battles in an isolated root."""
import hashlib
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    out = BASE / 'gov-frun-autosave-runtime-2'
    if out.exists():
        raise RuntimeError('runtime output already exists')
    out.mkdir()
    result = {'status': 'INCOMPLETE', 'scope': 'original Gov start handler and Windows D3D9; Switch NOT RUN'}
    try:
        fixture = BASE / 'gov-frun-runtime-1/native-input'
        output = out / 'output'
        output.mkdir()
        files = [fixture / 'CFG/robots/data.txt', fixture / 'CFG/robots/iface.txt', fixture / 'DATA/robots.pkg']
        before = {str(path): sha(path) for path in files}
        binary = BASE / 'gov-frun-autosave-link-2/gov-frun-autosave.exe'
        env = dict(os.environ, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='windows')
        env['PATH'] = str(BASE / 'deps/sdl2/SDL2-2.30.11/lib/x64') + ';' + str(BASE / 'deps/d3dx/build/native/release/bin/x64') + ';' + env.get('PATH', '')
        command = [str(binary), str(output), str(ROOT / 'windows/Space Rangers HD A War Apart'), str(fixture)]
        result.update(command=command, binary_sha256=sha(binary), fixture_sha256=before)
        print('autosave handler runtime started; see runtime.log', flush=True)
        with (out / 'runtime.log').open('w', encoding='utf8') as log:
            process = subprocess.run(command, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
        result['exit'] = process.returncode
        print('runtime exit', process.returncode, flush=True)
        if process.returncode:
            raise RuntimeError('reference process failed; see runtime.log')
        log = (out / 'runtime.log').read_text(encoding='utf8', errors='replace')
        assert log.count('ui_autosave_handler=CALLED') >= 2
        assert all('GOV_FRUN battle_index=' + str(i) + ' completed' in log for i in (1, 2))
        assert 'original_campaign_UI_script_host_cleanup=PASS' in log
        saves = list((output / 'rangers-user/config/save').glob('*'))
        result['save_files'] = [{'name': path.name, 'size': path.stat().st_size, 'sha256': sha(path)} for path in saves if path.is_file()]
        assert any(item['size'] > 0 and item['name'].lower() == 'autosave.sav' for item in result['save_files'])
        assert all(sha(Path(path)) == digest for path, digest in before.items())
        result['status'] = 'PASS; Gov start handler produced autosave and two battles completed; Switch NOT RUN'
    finally:
        (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')

if __name__ == '__main__':
    main()
