"""Run and verify the original screen dispatcher with two real battles."""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/windows-reference-rebuild-20261123'

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def main():
    surrender = sys.argv[1:] == ['--surrender']
    failure = sys.argv[1:] == ['--failure']
    oom = sys.argv[1:] == ['--oom']
    missing_map = sys.argv[1:] == ['--missing-map']
    concurrency = sys.argv[1:] == ['--concurrent-save']
    classification = sys.argv[1:] == ['--classification-save']
    worker_failure = sys.argv[1:] == ['--worker-failure'] or concurrency
    save_retry = sys.argv[1:] == ['--save-retry'] or worker_failure or classification
    unknown_transition = sys.argv[1:] == ['--unknown-transition']
    snapshot_regression = sys.argv[1:] == ['--snapshot-regression'] or unknown_transition or save_retry
    savefix_regression = sys.argv[1:] == ['--savefix-regression'] or snapshot_regression
    fixed_regression = sys.argv[1:] == ['--fixed-regression'] or savefix_regression
    failure = failure or oom or missing_map or unknown_transition
    menu = sys.argv[1:] == ['--menu']
    if sys.argv[1:] and not (surrender or failure or menu or fixed_regression):
        raise RuntimeError('unknown arguments')
    out = BASE / ('gov-frun-classification-save-runtime-1' if classification else 'gov-frun-concurrent-save-runtime-1' if concurrency else 'gov-frun-worker-failure-runtime-1' if worker_failure else 'gov-frun-save-retry-runtime-1' if save_retry else 'gov-frun-unknown-transition-runtime-2' if unknown_transition else 'gov-frun-snapshot-regression-runtime-1' if snapshot_regression else 'gov-frun-savefix-regression-runtime-1' if savefix_regression else 'gov-frun-fixed-regression-runtime-2' if fixed_regression else 'gov-frun-missing-map-runtime-5' if missing_map else 'gov-frun-oom-runtime-1' if oom else 'gov-frun-menu-runtime-2' if menu else 'gov-frun-failure-runtime-1' if failure else 'gov-frun-surrender-runtime-2' if surrender else 'gov-frun-dispatcher-runtime-1')
    if out.exists():
        raise RuntimeError('runtime output already exists')
    out.mkdir()
    result = {'status': 'INCOMPLETE', 'scope': 'Windows original screen dispatcher, Gov and D3D9; Switch NOT RUN'}
    try:
        fixture = BASE / 'gov-frun-runtime-1/native-input'
        output = out / 'output'
        output.mkdir()
        files = [fixture / 'CFG/robots/data.txt', fixture / 'CFG/robots/iface.txt', fixture / 'DATA/robots.pkg']
        before = {str(path): sha(path) for path in files}
        binary = BASE / ('gov-frun-classification-save-link-1' if classification else 'gov-frun-concurrent-save-link-1' if concurrency else 'gov-frun-worker-failure-link-1' if worker_failure else 'gov-frun-save-retry-link-1' if save_retry else 'gov-frun-unknown-transition-link-1' if unknown_transition else 'gov-frun-snapshot-regression-link-1' if snapshot_regression else 'gov-frun-savefix-regression-link-1' if savefix_regression else 'gov-frun-fixed-regression-link-2' if fixed_regression else 'gov-frun-missing-map-link-5' if missing_map else 'gov-frun-oom-link-1' if oom else 'gov-frun-menu-link-2' if menu else 'gov-frun-failure-link-1' if failure else 'gov-frun-surrender-link-2' if surrender else 'gov-frun-dispatcher-link-2') / 'gov-frun-dispatcher.exe'
        env = dict(os.environ, SDL_AUDIODRIVER='dummy', SDL_VIDEODRIVER='windows')
        env['PATH'] = str(BASE / 'deps/sdl2/SDL2-2.30.11/lib/x64') + ';' + str(BASE / 'deps/d3dx/build/native/release/bin/x64') + ';' + env.get('PATH', '')
        command = [str(binary), str(output), str(ROOT / 'windows/Space Rangers HD A War Apart'), str(fixture)]
        result.update(command=command, binary_sha256=sha(binary), fixture_sha256=before)
        print('dispatcher runtime started; see runtime.log', flush=True)
        with (out / 'runtime.log').open('w', encoding='utf8') as log:
            process = subprocess.run(command, cwd=out, env=env, stdout=log, stderr=subprocess.STDOUT)
        result['exit'] = process.returncode
        print('runtime exit', process.returncode, flush=True)
        if process.returncode:
            raise RuntimeError('reference process failed; see runtime.log')
        log = (out / 'runtime.log').read_text(encoding='utf8', errors='replace')
        markers = (('FAILURE planet_to_government=PASS', 'FAILURE original_Gov_start=PASS',
                    'FAILURE original_Pascal_exception=CAUGHT', 'FAILURE unknown_snapshot_rejected=PASS',
                    'FAILURE native_callback_snapshot_window_campaign_recovered=PASS no_fake_result=PASS') if unknown_transition else
                   ('FAILURE planet_to_government=PASS', 'FAILURE original_Gov_start=PASS',
                    'FAILURE missing_map_fixture=ARMED', 'FAILURE error_dialog_cancel=INPUT',
                    'FAILURE original_Pascal_exception=CAUGHT', 'FAILURE native_callback_snapshot_window_campaign_recovered=PASS no_fake_result=PASS') if missing_map else
                   ('FAILURE planet_to_government=PASS', 'FAILURE original_Gov_start=PASS',
                    'FAILURE injected_native_loop_callback=REACHED', 'FAILURE error_dialog_cancel=INPUT',
                    'FAILURE original_Pascal_exception=CAUGHT', 'FAILURE native_callback_snapshot_window_campaign_recovered=PASS no_fake_result=PASS') if failure else
                   ('SURRENDER planet_to_government=PASS', 'SURRENDER original_Gov_start=PASS',
                    'SURRENDER original_menu_confirmation=PASS', 'SURRENDER original_input_exit_and_statistics=PASS',
                    'SURRENDER original_Gov_relation_no_reward_history_statistics=PASS',
                    'SURRENDER dispatcher_return_to_planet=PASS', 'SURRENDER original_screen_state_loop=PASS') if surrender else
                   ('DISPATCH planet_to_government=1', 'DISPATCH planet_to_government=2',
                    'DISPATCH battle=1 original_Gov_start=CALL', 'DISPATCH battle=2 original_Gov_start=CALL',
                    'DISPATCH both_battles_returned_to_planet=PASS', 'DISPATCH original_screen_state_loop=PASS'))
        for marker in (*markers, 'original_campaign_UI_script_host_cleanup=PASS'):
            assert marker in log, marker
        if worker_failure:
            assert 'WORKER_FAILURE three_failures_preserve_previous_save=PASS' in log
            if concurrency:
                assert 'CONCURRENT_SAVE failed_manual_and_successful_auto=PASS' in log
        elif save_retry:
            assert ('CLASSIFICATION_FAILURE old_save_unchanged_all_three_handlers=PASS' if classification else 'SAVE_RETRY all_three_autosave_failures_return_to_Gov=PASS') in log
            assert log.count('|quest_item|vkDword|UNKNOWN|') >= 3
        if missing_map:
            assert 'FAILURE injected_native_loop_callback=REACHED' not in log
        if menu:
            for marker in ('MENU_BATTLE original_main_menu=REACHED', 'MENU_BATTLE original_new_game=START',
                           'MENU_BATTLE original_station_to_hangar=PASS', 'MENU_BATTLE original_hangar_takeoff=PASS',
                           'MENU_BATTLE original_landing_order=PASS',
                           'MENU_BATTLE original_menu_generation_to_battles=PASS'):
                assert marker in log, marker
        if not (surrender or failure):
            assert log.count('history_statistics_window=PASS') == 2
            assert log.count('COMPLETION_NATURAL_DIALOG') == 2
        saves = [] if unknown_transition else [output / 'rangers-user/config/save/AutoSave.sav']
        if not (surrender or failure):
            saves.append(output / 'rangers-user/config/save/AutoSave.sav.bak')
        assert all(path.is_file() and path.stat().st_size for path in saves)
        result['saves'] = [{'path': str(path), 'size': path.stat().st_size, 'sha256': sha(path)} for path in saves]
        assert all(sha(Path(path)) == digest for path, digest in before.items())
        if unknown_transition:
            assert '|quest_item|vkDword|UNKNOWN|' in log
            assert not (output / 'rangers-user/config/save/AutoSave.sav').exists()
        result['status'] = ('PASS; injected unknown rejected during Gov transition, world retained; Switch NOT RUN' if unknown_transition else
                            'PASS; missing map init failure, original Gov cancellation and restoration; Switch NOT RUN' if missing_map else
                            'PASS; injected std::bad_alloc, original Gov cancellation and restoration; Switch NOT RUN' if oom else
                            'PASS; original main menu, campaign generation and two battles through dispatcher; Switch NOT RUN' if menu else
                            'PASS; native loop injected exception, original Gov cancellation and restoration; Switch NOT RUN' if failure else
                            'PASS; surrender through original menu and screen dispatcher; Switch NOT RUN' if surrender else
                            'PASS; two battles through original screen dispatcher; Switch NOT RUN')
    finally:
        (out / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')

if __name__ == '__main__':
    main()
