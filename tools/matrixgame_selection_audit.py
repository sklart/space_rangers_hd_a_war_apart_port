"""Audit original selection/movement/fire and native alias ownership. No hardware claims."""
import json
import re
from pathlib import Path
from matrixgame_text_checks import sha

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/e2e538-selection'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    names = ('input/host', 'input/arm64', 'map-state/windows', 'map-state/host', 'map-state/arm64', 'viewport-clip/host', 'viewport-clip/arm64', 'windows', 'sdk')
    profiles = {name: read(BASE / name / 'result.json') for name in names}
    for name, result in profiles.items():
        expected = 'COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN' if name == 'sdk' else 'PASS'
        assert result['status'] == expected, name
        assert all(c['exit'] == 0 for c in result['commands']), name
        for source, digest in result['source_sha256'].items():
            assert sha(ROOT / source) == digest, (name, source)
        for source, digest in result.get('capture_sha256', {}).items():
            assert sha(ROOT / source) == digest, (name, source)
    windows = profiles['map-state/windows']['stdout']
    for marker in ('REAL_SELECTION mouse_drag=PASS', 'merged_mouse_pad=PASS focus_release_restore=PASS', 'mouse_held_after_pad_key_release=1 fire_order_after_pad_key_release=1', 'REAL_FORM_LOOP draws=8 elapsed=125', 'PrepareMap complete'):
        assert windows.count(marker) == 2, marker
    assert 'ACTUAL_MAP_HAL_RESULT=0 NATIVE_STATE_REJECTED_SESSIONS=0' in windows
    movements = [float(v) for v in re.findall(r'movement=([0-9.]+)', windows)]
    assert len(movements) == 2 and min(movements) > .01
    draws = sum(map(int, re.findall(r'ACTUAL_MAP_NATIVE_STATE draws=(\d+)', windows)))
    unique = sum(map(int, re.findall(r'ACTUAL_MAP_NATIVE_STATE draws=\d+ indexed=\d+ unique=(\d+)', windows)))
    for kind in ('host', 'arm64'):
        assert f'draws={draws} unique={unique} failures=0' in profiles['map-state/' + kind]['stdout']
        assert 'mouse_key_aliases=3 release_orders=checked' in profiles['input/' + kind]['stdout']
    baseline_dir = BASE / 'fire-alias-reproduced'
    baseline = read(baseline_dir / 'result.json')
    assert baseline['status'] == 'FAIL' and baseline['commands'][-1]['exit'] == 10
    baseline_text = (baseline_dir / 'runtime.log').read_text(encoding='utf-8')
    assert 'mouse_held_after_pad_key_release=0 fire_order_after_pad_key_release=0' in baseline_text
    assert 'releasing pad fire key interrupted mouse-owned original fire' in baseline_text
    assert sha(baseline_dir / 'actual-draw.exe') == baseline['binary_sha256']
    assert sha(BASE / 'map-state/windows/actual-draw.exe') == profiles['map-state/windows']['binary_sha256']
    match = re.search(r'ACTUAL_ORIGINAL_LINE_HAL_GLSL cases=(\d+) pixels=(\d+) mismatches=(\d+) max_delta=(\d+)', profiles['windows']['stdout'])
    assert match and int(match[3]) == 0 and int(match[4]) == 0
    assert 'VIEWPORT_TRIANGLE_CLIP_HAL_GLSL cases=24 mismatches=0' in profiles['windows']['stdout']
    assert 'RAW_LINE_HAL_GLSL cases=398 strips=64 degenerate=14 last0_mismatches=0 last1_mismatches=0 max_delta=0' in profiles['windows']['stdout']
    assert sha(BASE / 'windows/line-oracle.exe') == profiles['windows']['binary_sha256']
    line_baseline_dir = BASE / 'line-failure-diagnostic'
    line_baseline = read(line_baseline_dir / 'result.json')
    assert line_baseline['status'] == 'FAIL' and line_baseline['commands'][-1]['exit'] == 10
    assert 'cases=114 pixels=54720000 mismatches=6 max_delta=216' in line_baseline['stdout']
    assert sha(line_baseline_dir / 'line-oracle.exe') == line_baseline['binary_sha256']
    for capture, digest in line_baseline['capture_sha256'].items():
        assert profiles['windows']['capture_sha256'][capture] == digest and sha(ROOT / capture) == digest
    assert not list(BASE.rglob('*.nro'))
    text = read(ROOT / 'reports/matrixgame-text-current-evidence.json')
    assert text['status'] == 'TEXT SOFTWARE RECHECK PASS; NATIVE INIT HARDWARE PENDING'
    generated = read(BASE / 'sdk/source/native-engine-source.json')['files_sha256']
    assert generated == read(ROOT / 'build/matrixgame/e2e537-lines/sdk/source/native-engine-source.json')['files_sha256']
    for source, digest in generated.items():
        assert sha(BASE / 'sdk/source' / source) == digest, source
    source_names = {p for result in profiles.values() for p in result['source_sha256']}
    source_names |= {'tools/matrixgame_selection_audit.py', 'tools/matrixgame_text_recheck_audit.py', 'tools/matrixgame_actual_draw_checks.py', 'tools/matrixgame_cursor_checks.py',
                     'docs/matrixgame-init-sequence.md', 'docs/matrixgame-readiness-plan.md', 'reports/matrixgame-e2e538-selection.md', 'reports/matrixgame-text-current.md'}
    result = dict(status='E2E-5.38 INPUT AND VIEWPORT CLIP SOFTWARE PASS; SWITCH RUNTIME PARTIAL', profiles=profiles,
                  source_sha256={p: sha(ROOT / p) for p in sorted(source_names)},
                  actual_draws=draws, unique_snapshots=unique, actual_line_cases=int(match[1]), compared_RGBA_pixels=int(match[2]),
                  failing_line_cases=int(match[3]), max_channel_delta=int(match[4]),
                  line_baseline_failure_preserved=True, line_capture_payloads_unchanged=True,
                  original_robot_movement=movements, baseline_failure_preserved=True,
                  text_evidence='reports/matrixgame-text-current-evidence.json',
                  original_generated_files_unchanged=len(generated),
                  native_map_loaded=False, native_frame_presented=False,
                  Switch_hardware='NOT RUN / HARDWARE_PENDING', first_native_runtime_blocker='NOT OBSERVED: no Switch execution',
                  closed_software_blocker='CMinimap::Draw closed LINESTRIP after robot movement: implicit logical viewport clip preserved alongside half-pixel physical viewport',
                  NRO_created=False, commit=False, push=False,
                  open_gates=text['open_gates'])
    (ROOT / 'reports/matrixgame-e2e538-selection-evidence.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('status', 'actual_draws', 'unique_snapshots', 'actual_line_cases', 'failing_line_cases', 'max_channel_delta')}, ensure_ascii=False))


if __name__ == '__main__':
    main()
