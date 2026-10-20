"""Real asset-backed host route; legacy driver evidence, not controller/hardware PASS."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import uuid

import e2e5_host_game_probe as probe

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'port/switch/build/e2e5-host-game/optimized'
OUTPUT = ROOT / 'build/e2e515/main-route'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--config-root', type=Path, required=True)
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    result_path = OUTPUT / 'result.json'
    result_path.write_text(json.dumps({'status': 'RUNNING'}))
    before = {name: hashlib.sha256((args.config_root / name).read_bytes()).hexdigest()
              for name in ('CFG.TXT', 'Lang.txt', 'newgame.txt')}
    probe.RUNS = OUTPUT / 'runs'
    frames = OUTPUT / 'frames' / uuid.uuid4().hex
    frames.mkdir(parents=True)
    os.environ.update(E2E5_HOST_MAP_MENU_CYCLES='1', E2E511_HOST_FRAMES=str(frames))
    binary = OUTPUT / 'game-host'
    shutil.copyfile(BUILD / 'original-game-host', binary)
    binary.chmod(0o755)
    result = probe.run(binary, args.game_root, BUILD / 'okgf', 180, None, True,
        auto_start_game=True, auto_select_dialog=True, auto_advance_dialog=True,
        auto_takeoff=True, cfg_file=args.config_root / 'CFG.TXT',
        new_game_file=args.config_root / 'newgame.txt', lang_file=args.config_root / 'Lang.txt')
    (OUTPUT / 'probe.json').write_text(json.dumps(result, indent=2) + '\n')
    log = Path(result['stage']) / 'sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log'
    lines = log.read_text(errors='replace') if log.exists() else ''
    after = {name: hashlib.sha256((args.config_root / name).read_bytes()).hexdigest() for name in before}
    dispatches = result['screen_dispatches']
    forbidden = ('input owner cancellation failed', 'input loop stack unwind mismatch', 'BOOT FAILED')
    route_complete = (all(screen in dispatches for screen in (1, 33, 3, 20, 4, 16)) and
                      dispatches.count(25) >= 5 and dispatches.count(16) >= 6 and
                      result['generation_complete'] and result['introduction_continue_enabled'])
    passed = ('HOST map menu cycles PASS count=5' in lines and route_complete and
              not result['timed_out'] and not result['crashed'] and
              not any(marker in lines for marker in forbidden) and before == after)
    report = {'status': 'PASS' if passed else 'FAIL',
              'scope': 'real game + legacy keyboard/mouse driver; not controller-only route or hardware proof',
              'five_map_menu_cycles': 'HOST map menu cycles PASS count=5' in lines,
              'route_complete': route_complete,
              'driver_stopped_process': result['stopped_by_probe'], 'returncode': result['returncode'],
              'normal_shutdown_verified': False,
              'source_configs_unchanged': before == after,
              'binary_sha256': result['binary_sha256'], 'stage': result['stage'],
              'timed_out': result['timed_out'], 'screen_dispatches': result['screen_dispatches'],
              'frames_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in frames.glob('*.ppm')}}
    result_path.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'frames_sha256'}, indent=2), flush=True)
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        result_path = OUTPUT / 'result.json'
        if result_path.exists() and json.loads(result_path.read_text()).get('status') == 'RUNNING':
            result_path.write_text(json.dumps({'status': 'FAIL', 'error': str(error)}, indent=2) + '\n')
        raise
