"""Run an isolated menu/Gov reference with real mouse-driven dialog choices."""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'
two_battles = sys.argv[1:] == ['--two-battles']
if sys.argv[1:] and not two_battles:
    raise RuntimeError('unknown arguments')
OUT = BASE / ('gov-frun-menu-two-ui-quest-runtime-2' if two_battles else 'gov-frun-menu-ui-quest-runtime-2')
LINK = BASE / ('gov-frun-menu-two-ui-quest-link-2' if two_battles else 'gov-frun-menu-ui-quest-link-4')
if OUT.exists():
    raise RuntimeError('output already exists')
OUT.mkdir()
result = {'status': 'INCOMPLETE', 'scope': 'Windows D3D9 reference; no Switch'}

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

try:
    linked = json.loads((LINK / 'result.json').read_text(encoding='utf-8'))
    binary = LINK / 'gov-frun-dispatcher.exe'
    assert linked['status'].startswith('PASS') and sha(binary) == linked['binary_sha256']
    fixture = BASE / 'gov-frun-runtime-1/native-input'
    files = [fixture / 'CFG/robots/data.txt', fixture / 'CFG/robots/iface.txt', fixture / 'DATA/robots.pkg']
    before = {str(path): sha(path) for path in files}
    output = OUT / 'output'; output.mkdir()
    env = dict(os.environ, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='windows')
    env['PATH'] = str(BASE / 'deps/sdl2/SDL2-2.30.11/lib/x64') + ';' + str(BASE / 'deps/d3dx/build/native/release/bin/x64') + ';' + env.get('PATH', '')
    command = [str(binary), str(output), str(ROOT / 'windows/Space Rangers HD A War Apart'), str(fixture)]
    result.update(command=command, binary_sha256=sha(binary), fixture_sha256=before)
    print('original UI test started', flush=True)
    with (OUT / 'runtime.log').open('w', encoding='utf-8') as log:
        process = subprocess.run(command, cwd=OUT, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=600)
    result['exit'] = process.returncode
    log = (OUT / 'runtime.log').read_text(encoding='utf-8', errors='replace')
    assert process.returncode == 0, 'reference failed; inspect runtime.log'
    assert all(sha(Path(path)) == digest for path, digest in before.items())
    end_marker = 'UI_QUEST session_returned_to_planet' if two_battles else 'UI_QUEST returned_to_planet=PASS'
    for marker in ('MENU_BATTLE original_main_menu=REACHED', 'MENU_BATTLE original_new_game=START',
                   end_marker, 'original_campaign_UI_script_host_cleanup=PASS'):
        assert marker in log, marker
    if 'UI_QUEST no_visible_choice' in log or 'UI_QUEST turn_wait_timeout' in log:
        result['status'] = 'INCONCLUSIVE; original quest choice or turn progression unavailable before deadline; normal cleanup; Switch NOT RUN'
    else:
        for marker in ('UI_QUEST requested_through_mouse=PASS', 'accepted_through_mouse=PASS',
                       'UI_QUEST support_mouse_queued', 'COMPLETION_NATURAL_DIALOG',
                       'history_statistics_window=PASS'):
            assert marker in log, marker
        if two_battles:
            assert log.count('UI_QUEST support_mouse_queued') == 2
            assert log.count('history_statistics_window=PASS') == 2
            assert 'UI_QUEST cooldown_elapsed' in log
            assert 'DISPATCH both_battles_returned_to_planet=PASS' in log
        result['status'] = ('PASS; two naturally offered battles through original Gov mouse choices; Switch NOT RUN' if two_battles else
                            'PASS; one naturally offered battle through original Gov mouse choices; Switch NOT RUN')
finally:
    (OUT / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
