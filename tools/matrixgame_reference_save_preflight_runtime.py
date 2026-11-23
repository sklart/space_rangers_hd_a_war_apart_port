"""Probe generated campaigns until a save rejection can be diagnosed."""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'
dangling = sys.argv[1:] == ['--gov-item-dangling']
calibrate = sys.argv[1:] == ['--gov-item-calibrate'] or dangling
trace_v3_retry = sys.argv[1:] == ['--gov-item-trace-v3-retry']
trace_v3 = sys.argv[1:] == ['--gov-item-trace-v3'] or calibrate or trace_v3_retry
trace_v2 = sys.argv[1:] == ['--gov-item-trace-v2'] or trace_v3
trace_retry = sys.argv[1:] == ['--gov-item-trace-retry']
item_trace = sys.argv[1:] == ['--gov-item-trace'] or trace_retry or trace_v2
fixed_gov = sys.argv[1:] == ['--gov-fixed'] or item_trace
gov_preflight = sys.argv[1:] == ['--gov'] or fixed_gov
if sys.argv[1:] and not gov_preflight:
    raise RuntimeError('unknown arguments')
out = BASE / ('gov-frun-item-dangling-runtime-1' if dangling else 'gov-frun-item-calibrate-runtime-1' if calibrate else 'gov-frun-gov-item-trace-runtime-5' if trace_v3_retry else 'gov-frun-gov-item-trace-runtime-4' if trace_v3 else 'gov-frun-gov-item-trace-runtime-3' if trace_v2 else 'gov-frun-gov-item-trace-runtime-2' if trace_retry else 'gov-frun-gov-item-trace-runtime-1' if item_trace else 'gov-frun-gov-preflight-runtime-4' if fixed_gov else 'gov-frun-gov-preflight-runtime-2' if gov_preflight else 'gov-frun-save-preflight-runtime-1')
if out.exists():
    raise RuntimeError('output already exists')
out.mkdir()
result = {'status': 'INCOMPLETE', 'runs': [], 'scope': 'isolated Windows generated campaigns; no Switch'}

try:
    binary = BASE / ('gov-frun-item-dangling-link-1' if dangling else 'gov-frun-item-calibrate-link-2' if calibrate else 'gov-frun-gov-item-trace-link-3' if trace_v3 else 'gov-frun-gov-item-trace-link-2' if trace_v2 else 'gov-frun-gov-item-trace-link-1' if item_trace else 'gov-frun-gov-preflight-link-4' if fixed_gov else 'gov-frun-gov-preflight-link-2' if gov_preflight else 'gov-frun-save-preflight-link-1') / 'gov-frun-reload.exe'
    fixture = BASE / 'gov-frun-runtime-1/native-input'
    env = dict(os.environ, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='windows')
    env['PATH'] = str(BASE / 'deps/sdl2/SDL2-2.30.11/lib/x64') + ';' + str(BASE / 'deps/d3dx/build/native/release/bin/x64') + ';' + env.get('PATH', '')
    for index in range(1, 2 if calibrate else 25 if gov_preflight else 13):
        run = out / str(index); run.mkdir()
        output = run / 'output'; output.mkdir()
        command = [str(binary), str(output), str(ROOT / 'windows/Space Rangers HD A War Apart'), str(fixture)]
        print('preflight', index, 'started', flush=True)
        with (run / 'runtime.log').open('w', encoding='utf8') as stream:
            process = subprocess.run(command, cwd=run, env=env, stdout=stream, stderr=subprocess.STDOUT)
        log = (run / 'runtime.log').read_text(encoding='utf8', errors='replace')
        game_log_path = output / 'rangers-user/logs/game.log'
        game_log = game_log_path.read_text(encoding='utf8', errors='replace') if game_log_path.exists() else ''
        rejected = 'SAVE_PREFLIGHT original_save=REJECTED' in log
        accepted = 'SAVE_PREFLIGHT original_save=ACCEPTED' in log
        result['runs'].append({'index': index, 'exit': process.returncode, 'accepted': accepted, 'rejected': rejected,
                               'unknown_diagnostic': [line for line in game_log.splitlines() if 'E2E5 save unknown' in line]})
        if process.returncode or accepted == rejected or 'original_campaign_UI_script_host_cleanup=PASS' not in log:
            raise RuntimeError('unexpected process result; inspect run ' + str(index))
        if gov_preflight and 'SAVE_PREFLIGHT original_Gov_and_previews=PASS' not in log:
            raise RuntimeError('Gov preview context absent; inspect run ' + str(index))
        print('preflight', index, 'rejected' if rejected else 'accepted', flush=True)
        if calibrate:
            import re
            assignment = re.search(r'quest_item_assignment matches=1 observed=1 live=1 id=(\d+) generation=(\d+)', log)
            destroyed = re.search(r'quest_item_lifetime=DESTROYED id=(\d+) generation=(\d+)', log)
            assert assignment and destroyed and assignment.groups() == destroyed.groups()
            assert 'ITEM_CALIBRATION restored=PASS id=' + assignment[1] in log
            if dangling:
                assert 'ITEM_DANGLING real_preflight_rejected=PASS' in log
                assert '|quest_item|vkDword|UNKNOWN|' in log
            result['status'] = 'PASS; traced live assignment and destruction of same real item; Switch NOT RUN'
            break
        if rejected and (not trace_v3 or 'variable=quest_item' in game_log):
            result['status'] = 'REPRODUCED; inspect game.log and runtime.log; no save assertions weakened'
            break
    else:
        result['status'] = 'INCONCLUSIVE; no rejection in generated campaigns'
finally:
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
