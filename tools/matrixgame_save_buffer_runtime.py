"""Run controlled save failures against real isolated Rangers buffers."""
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'
unknown = sys.argv[1:] == ['--snapshot-unknown']
snapshot = sys.argv[1:] in (['--snapshot'], ['--snapshot-fixed']) or unknown
fixed = sys.argv[1:] in (['--fixed'], ['--snapshot-fixed']) or unknown
if sys.argv[1:] and not (fixed or snapshot):
    raise RuntimeError('unknown arguments')
out = BASE / ('snapshot-prep-unknown-runtime-1' if unknown else 'snapshot-prep-fixed-runtime-1' if snapshot and fixed else 'snapshot-prep-baseline-runtime-1' if snapshot else 'save-buffer-fixed-runtime-2' if fixed else 'save-buffer-baseline-runtime-1')
if out.exists(): raise RuntimeError('output already exists')
out.mkdir(); output = out / 'output'; output.mkdir()
result = {'status': 'INCOMPLETE', 'scope': 'Windows real EC_Buf and isolated campaign; Switch NOT RUN'}
try:
    binary = BASE / ('snapshot-prep-unknown-link-1' if unknown else 'snapshot-prep-fixed-link-1' if snapshot and fixed else 'snapshot-prep-baseline-link-2' if snapshot else 'save-buffer-fixed-link-2' if fixed else 'save-buffer-baseline-link-1') / 'gov-frun-reload.exe'
    fixture = BASE / 'gov-frun-runtime-1/native-input'
    env = dict(os.environ, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='windows')
    if fixed: env['SRHD_EXPECT_CLEAN_BUFFERS'] = '1'
    env['PATH'] = str(BASE / 'deps/sdl2/SDL2-2.30.11/lib/x64') + ';' + str(BASE / 'deps/d3dx/build/native/release/bin/x64') + ';' + env.get('PATH', '')
    command = [str(binary), str(output), str(ROOT / 'windows/Space Rangers HD A War Apart'), str(fixture)]
    print('save buffer test started', flush=True)
    with (out / 'runtime.log').open('w', encoding='utf8') as log:
        process = subprocess.run(command, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
    result['exit'] = process.returncode
    if process.returncode: raise RuntimeError('see runtime.log')
    log = (out / 'runtime.log').read_text(encoding='utf8', errors='replace')
    import re
    assert 'original_campaign_UI_script_host_cleanup=PASS' in log
    if unknown:
        assert 'SNAPSHOT_UNKNOWN classifier_rejected_world_and_buffer_restored=PASS' in log
        assert 'SNAPSHOT_UNKNOWN subsequent_real_snapshot_restore=PASS' in log
        assert '|quest_item|vkDword|UNKNOWN|' in log
        result['status'] = 'PASS; real preflight rejects injected unknown and retry restores; Switch NOT RUN'
    elif snapshot:
        matches = re.findall(r'SNAPSHOT_PREP_FAILURE point=(\d) live_delta=(-?\d+) buffer=(\d) save_count_delta=(-?\d+)', log)
        assert len(matches) == 2
        result['boundaries'] = {point: {'live_delta': int(live), 'buffer': int(buffer), 'save_count_delta': int(count)}
                                for point, live, buffer, count in matches}
        if fixed:
            assert all(row == {'live_delta': 0, 'buffer': 0, 'save_count_delta': 0} for row in result['boundaries'].values())
            assert 'SNAPSHOT_PREP subsequent_real_snapshot_restore=PASS existing_snapshot_preserved=PASS' in log
            result['status'] = 'PASS; snapshot preparation failure rolls back and real retry restores; Switch NOT RUN'
        else:
            assert result['boundaries']['4']['buffer'] == 1 and result['boundaries']['5']['save_count_delta'] == -1
            result['status'] = 'FAIL REPRODUCED; partial snapshot and save-count mutation retained'
    else:
        matches = re.findall(r'SAVE_BUFFER_FAILURE point=(\d) live_delta=(-?\d+)', log)
        assert len(matches) == 2 and 'SAVE_BUFFER_SUCCESS live_delta=0' in log
        result['deltas'] = {key: int(value) for key, value in matches}
        if fixed:
            assert result['deltas'] == {'1': 0, '2': 0}
            assert 'SAVE_BUFFER_AFTER_QUEUE worker_ownership_and_reload=PASS' in log
            result['status'] = 'PASS; both failure boundaries release buffers, normal save succeeds; Switch NOT RUN'
        else:
            assert result['deltas']['1'] > 0 and result['deltas']['2'] > 0
            result['status'] = 'FAIL REPRODUCED; save failure retains buffers'
    print(result['status'], flush=True)
finally:
    (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
