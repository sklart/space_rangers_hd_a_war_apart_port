"""Asset-backed original game route driven only by injected raw pad buttons."""
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'port/switch/build/e2e5-host-game/optimized'
OUTPUT = ROOT / 'build/e2e515/controller-route'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--config-root', type=Path, required=True)
    args = parser.parse_args()
    stage = OUTPUT / 'runs' / uuid.uuid4().hex
    config = stage / 'user/config'
    config.mkdir(parents=True)
    original = {}
    for name in ('CFG.TXT', 'Lang.txt', 'newgame.txt'):
        source = args.config_root / name
        original[name] = hashlib.sha256(source.read_bytes()).hexdigest()
        shutil.copyfile(source, config / name)
    binary = (stage / 'original-game-host').resolve()
    shutil.copyfile(BUILD / 'original-game-host', binary)
    binary.chmod(0o755)
    env = os.environ.copy()
    # This driver must not run together with an inherited legacy key/click rig.
    for name in tuple(env):
        if name.startswith(('E2E5_HOST_', 'E2E511_HOST_', 'E2E513_HOST_', 'E2E59_HOST_')):
            del env[name]
    frames = stage / 'frames'
    frames.mkdir()
    env.update(E2E5_GAME_ROOT=str(args.game_root.resolve()), SDL_VIDEODRIVER='dummy',
               SDL_AUDIODRIVER='dummy', LD_LIBRARY_PATH=str((BUILD / 'okgf').resolve()),
               E2E515_HOST_PAD_MAILBOX='1', E2E515_FOCUS_TRACE='1',
               E2E511_HOST_FRAMES=str(frames.resolve()))
    log = stage / 'sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log'
    sequence = 0
    last_log = ''
    report = {'status': 'RUNNING', 'stage': str(stage), 'hardware_executed': False,
              'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
              'normal_shutdown_verified': False}
    result_path = OUTPUT / 'result.json'
    result_path.write_text(json.dumps(report, indent=2))
    with (stage / 'stdout.log').open('wb') as out, (stage / 'stderr.log').open('wb') as err:
        process = subprocess.Popen([str(binary)], cwd=stage, env=env, stdout=out, stderr=err)

        def lines():
            nonlocal last_log
            try:
                if log.exists():
                    last_log = log.read_text(errors='replace')
            except OSError as error:
                # DrvFS may briefly return ENODATA while the game flushes this file.
                if error.errno != errno.ENODATA:
                    raise
            return last_log

        def wait(predicate, description, timeout=40):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                text = lines()
                if any(marker in text for marker in ('BOOT FAILED', 'input owner cancellation failed',
                                                      'input loop stack unwind mismatch')):
                    raise RuntimeError('Game failure while ' + description)
                if process.poll() is not None:
                    raise RuntimeError(f'Game exited {process.returncode} while {description}')
                if predicate(text):
                    return text
                time.sleep(.05)
            raise RuntimeError('Timeout: ' + description)

        def state():
            text = lines()
            records = list(re.finditer(r'controller graph scope=(\d+) selected=(\d+) count=(\d+)', text))
            for match in reversed(records):
                tail = text[match.end():]
                if f'controller graph ready scope={match[1]}' in tail:
                    return int(match[1]), int(match[2]), int(match[3])
            return 0, 0, 0

        def raw(mask, duration=.17):
            nonlocal sequence
            sequence += 1
            at = len(lines())
            temporary = stage / 'pad-input.tmp'
            temporary.write_text(f'{sequence} {mask}\n')
            temporary.replace(stage / 'pad-input.txt')
            wait(lambda text: f'HOST raw pad sequence={sequence} mask={mask}' in text, 'raw HID delivery')
            action = {1: 'Confirm', 2: 'Cancel', 8: 'NavigateDown', 32: 'NavigateRight',
                      64: 'OpenControllerSettings', 128: 'ToggleNavigationMode', 256: 'Inspect'}.get(mask)
            if action:
                wait(lambda text: f'action={action} edge=pressed' in text[at:],
                     'game polls raw button ' + action)
            time.sleep(duration)

        def pulse(mask, duration=.17):
            raw(mask, duration)
            raw(0)

        try:
            wait(lambda text: 'screen first presented id=1 ' in text and state()[2] > 0,
                 'main menu and native focus graph', 80)
            cycles = []
            for cycle in range(5):
                old_scope = state()[0]
                at = len(lines())
                # A remains held across the original NewGame handler and context change.
                pulse(1, .9)
                wait(lambda text: 'screen dispatch id=33 ' in text[at:] and state()[0] != old_scope,
                     'new-game settings opened by A')
                text = lines()[at:]
                if 'screen dispatch id=3 ' in text or 'generation status=complete' in text:
                    raise RuntimeError('Held A leaked through to world generation')
                confirms = text.count('controller native action=Confirm ')
                if confirms != 1:
                    raise RuntimeError(f'Expected one native Confirm, observed {confirms}')
                settings_scope = state()[0]
                pulse(2)
                wait(lambda text: state()[0] not in (old_scope, settings_scope) and
                     'screen dispatch id=1 ' in text[at:] and
                     'screen Run first frame attempted' in text[text.rfind('screen dispatch id=1 '):],
                     'B returns to main menu')
                cycles.append({'cycle': cycle + 1, 'native_confirms': confirms,
                               'held_A_did_not_start_generation': True})

            def open_preferences():
                parent = state()[0]
                at = len(lines())
                pulse(64)
                wait(lambda text: state()[0] != parent and state()[2] > 0 and
                     'screen Run first frame attempted' in text[at:],
                     'native controller settings dialog')
                return parent

            parent = open_preferences()
            pulse(8)  # deadzone
            pulse(8)  # pointer speed
            pulse(32)  # 600 -> 650
            pulse(1)
            settings_file = config / 'controller.ini'
            wait(lambda text: settings_file.exists() and '650' in settings_file.read_text(),
                 'controller settings verified write')
            wait(lambda text: state()[0] == parent, 'settings return to parent focus')
            saved = settings_file.read_bytes()
            parent = open_preferences()
            pulse(32)  # pending Cursor
            pulse(2)   # cancel retains Hybrid and persisted bytes
            wait(lambda text: state()[0] == parent, 'settings cancel')
            if settings_file.read_bytes() != saved:
                raise RuntimeError('Cancel modified persisted controller settings')
            parent = open_preferences()
            pulse(32)  # Cursor
            pulse(1)
            wait(lambda text: state()[0] == parent and settings_file.read_text().split()[1] == '1',
                 'Cursor profile applied')
            pulse(128)  # L3 recovers focus without a mouse.
            at = len(lines())
            pulse(1)
            wait(lambda text: 'screen dispatch id=33 ' in text[at:], 'L3 restored native focus in Cursor profile')
            pulse(2)
            wait(lambda text: 'screen dispatch id=1 ' in text[at:] and
                 'screen Run first frame attempted' in text[text.rfind('screen dispatch id=1 '):],
                 'Cursor profile new-game cancellation')
            parent = open_preferences()
            pulse(256)  # reset pending values
            pulse(1)
            wait(lambda text: state()[0] == parent and settings_file.read_text().split() ==
                 ['1', '0', '5500', '600', '0', '0', '0', '0'], 'Reset defaults applied')
            report.update(status='PASS', cycles=cycles,
                          settings_apply_cancel_reset=True, cursor_focus_recovery=True,
                          scope='raw pad only; five menu/new-game cycles and native settings dialog')
        except Exception as error:
            report.update(status='FAIL', error=str(error))
        finally:
            process.kill()
            process.wait()
            report['returncode'] = process.returncode
            report['stopped_by_runner'] = True
    report['source_configs_unchanged'] = all(
        hashlib.sha256((args.config_root / name).read_bytes()).hexdigest() == digest
        for name, digest in original.items())
    if not report['source_configs_unchanged']:
        report.update(status='FAIL', error='Original configuration changed')
    result_path.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2), flush=True)
    if report['status'] != 'PASS':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
