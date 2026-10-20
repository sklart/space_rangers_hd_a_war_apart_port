"""Asset-backed original game route driven only by injected raw pad buttons."""
import argparse
from collections import deque
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
    parser.add_argument('--campaign', action='store_true', help='Continue via native focus, OSK and world generation')
    parser.add_argument('--save', action='store_true', help='After takeoff, save from the original UI using Unicode OSK')
    parser.add_argument('--load-save', type=Path, help='Load a copied build fixture in a separate fresh process')
    args = parser.parse_args()
    if args.save and not args.campaign:
        parser.error('--save requires --campaign')
    if args.load_save and (args.campaign or args.save):
        parser.error('--load-save is a separate fresh-process route')
    global OUTPUT
    if args.campaign:
        OUTPUT = ROOT / 'build/e2e515/controller-campaign'
    if args.save:
        OUTPUT = ROOT / 'build/e2e515/controller-save'
    if args.load_save:
        OUTPUT = ROOT / 'build/e2e515/controller-reload'
    stage = OUTPUT / 'runs' / uuid.uuid4().hex
    config = stage / 'user/config'
    config.mkdir(parents=True)
    original = {}
    for name in ('CFG.TXT', 'Lang.txt', 'newgame.txt'):
        source = args.config_root / name
        original[name] = hashlib.sha256(source.read_bytes()).hexdigest()
        shutil.copyfile(source, config / name)
    source_save = None
    source_digest = None
    if args.load_save:
        source_save = args.load_save.resolve(strict=True)
        if ((ROOT / 'build') not in source_save.parents or args.load_save.is_symlink() or
                not source_save.is_file() or source_save.suffix.lower() != '.sav'):
            raise ValueError('--load-save must be an isolated build fixture, never an original user save')
        source_digest = hashlib.sha256(source_save.read_bytes()).hexdigest()
        target = config / 'save' / source_save.name
        target.parent.mkdir(parents=True)
        shutil.copyfile(source_save, target)
        if hashlib.sha256(target.read_bytes()).hexdigest() != source_digest:
            raise RuntimeError('Copied save fixture hash mismatch')
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
               E2E515_FOCUS_SNAPSHOT=str((stage / 'focus.txt').resolve()),
               E2E511_HOST_FRAMES=str(frames.resolve()))
    if args.campaign:
        env['E2E515_HOST_ROUTE_OSK'] = '1'
    log = stage / 'sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log'
    game_log = config / '########.log'
    sequence = 0
    last_log = ''
    report = {'status': 'RUNNING', 'stage': str(stage), 'hardware_executed': False,
              'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
              'normal_shutdown_verified': False}
    if source_save:
        report.update(source_save_path=str(source_save), source_save_sha256=source_digest)
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

        def manual_committed():
            try:
                return game_log.exists() and 'E2E5 manual save committed path=' in game_log.read_text(errors='replace')
            except OSError as error:
                if error.errno != errno.ENODATA:
                    raise
                return False

        def wait(predicate, description, timeout=40):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                text = lines()
                if any(marker in text for marker in ('BOOT FAILED', 'input owner cancellation failed',
                                                      'input loop stack unwind mismatch', 'FAIL HOST')):
                    raise RuntimeError('Game failure while ' + description)
                if process.poll() is not None:
                    raise RuntimeError(f'Game exited {process.returncode} while {description}')
                if predicate(text):
                    return text
                time.sleep(.05)
            raise RuntimeError('Timeout: ' + description)

        def graph():
            try:
                text = (stage / 'focus.txt').read_text()
            except OSError as error:
                if error.errno not in (errno.ENODATA, errno.ENOENT):
                    raise
                return 0, 0, []
            records = list(re.finditer(r'controller graph scope=(\d+) selected=(\d+) count=(\d+)', text))
            for match in reversed(records):
                tail = text[match.end():]
                if f'controller graph ready scope={match[1]}' in tail:
                    body = tail[:tail.index(f'controller graph ready scope={match[1]}')]
                    nodes = [(int(m[1]), tuple(map(int, m[2].split(','))), m[3]) for m in
                             re.finditer(r'controller node scope=' + match[1] + r' id=(\d+) box=([-\d,]+) name=(\S*)', body)]
                    if len(nodes) == int(match[3]):
                        return int(match[1]), int(match[2]), nodes
            return 0, 0, []

        def state():
            scope, selected, nodes = graph()
            return scope, selected, len(nodes)

        def raw(mask, duration=.17):
            nonlocal sequence
            sequence += 1
            at = len(lines())
            temporary = stage / 'pad-input.tmp'
            temporary.write_text(f'{sequence} {mask}\n')
            temporary.replace(stage / 'pad-input.txt')
            wait(lambda text: f'HOST raw pad sequence={sequence} mask={mask}' in text, 'raw HID delivery')
            action = {1: 'Confirm', 2: 'Cancel', 4: 'NavigateUp', 8: 'NavigateDown',
                      16: 'NavigateLeft', 32: 'NavigateRight',
                      64: 'OpenControllerSettings', 128: 'ToggleNavigationMode', 256: 'Inspect'}.get(mask)
            if action:
                wait(lambda text: f'action={action} edge=pressed' in text[at:],
                     'game polls raw button ' + action)
            time.sleep(duration)

        def pulse(mask, duration=.17):
            raw(mask, duration)
            raw(0)

        def select(name):
            wait(lambda text: any(node[2] == name for node in graph()[2]), 'focus target ' + name)
            scope, selected, nodes = graph()
            target = next(node[0] for node in nodes if node[2] == name)
            boxes = {node[0]: node[1] for node in nodes}

            def neighbor(origin, key):
                x, y, w, h = boxes[origin]
                horizontal, sign = key in (16, 32), -1 if key in (4, 16) else 1
                choices, wrapped = [], []
                for identity, (nx, ny, nw, nh) in boxes.items():
                    if identity == origin:
                        continue
                    dx, dy = 2 * nx + nw - (2 * x + w), 2 * ny + nh - (2 * y + h)
                    along, across = (dx if horizontal else dy) * sign, abs(dy if horizontal else dx)
                    if along > 0:
                        choices.append((along + across * 4, identity))
                    elif along < 0:
                        wrapped.append((along + across * 8, identity))
                candidates = choices or wrapped
                return min(candidates, key=lambda pair: pair[0])[1] if candidates else origin

            pending, seen = deque([(selected, [])]), {selected}
            path = None
            while pending:
                origin, steps = pending.popleft()
                if origin == target:
                    path = steps
                    break
                for key in (4, 8, 16, 32):
                    reached = neighbor(origin, key)
                    if reached not in seen:
                        seen.add(reached)
                        pending.append((reached, steps + [(key, reached)]))
            if path is None:
                raise RuntimeError('Native focus target unreachable: ' + name)
            # This is a navigation plan, not a mocked game result: observe every
            # actual focus change and fail if the game takes a different edge.
            for key, reached in path:
                pulse(key)
                wait(lambda text: state()[:2] == (scope, reached), 'actual focus step to ' + name)

        try:
            wait(lambda text: 'screen first presented id=1 ' in text and state()[2] > 0,
                 'main menu and native focus graph', 80)
            if args.load_save is None:
                cycles = []
                if not args.campaign:
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
                if args.campaign:
                    report['status'] = 'RUNNING'
                    at = len(lines())
                    pulse(1)
                    wait(lambda text: 'screen dispatch id=33 ' in text[at:] and
                         'screen Run first frame attempted' in text[text.rfind('screen dispatch id=33 '):],
                         'campaign new-game settings')
                    select('PlayerName')
                    pulse(1)
                    wait(lambda text: 'HOST OSK fixture call=1 result=Accept' in text, 'player name OSK')
                    pulse(1)
                    wait(lambda text: 'HOST OSK fixture call=2 initial_matches=1 result=Cancel' in text,
                         'real TEdit retained Unicode player name on reopen')
                    report['player_name_unicode_reopen_cancel'] = True
                    select('Ok')
                    at = len(lines())
                    pulse(1)
                    wait(lambda text: 'screen dispatch id=3 ' in text[at:] and
                         'introduction status=continue-enabled' in text[at:], 'real world generation', 180)
                    report['world_generation_complete'] = True
                    select('Ok')
                    at = len(lines())
                    pulse(1)
                    wait(lambda text: 'screen dispatch id=20 ' in text[at:], 'character introduction dialog')
                    ready_after = at
                    choices_taken = 0
                    for _ in range(8):
                        wait(lambda text: 'screen dispatch id=4 ' in text[at:] or
                             ('dialog choice ready' in text[ready_after:] and
                              any(node[2].startswith('DialogChoice') for node in graph()[2])),
                             'visible native character choice or hangar')
                        if 'screen dispatch id=4 ' in lines()[at:]:
                            break
                        available = [node for node in graph()[2] if node[2].startswith('DialogChoice')]
                        select(min(available, key=lambda node: node[1][1])[2])
                        callbacks = lines().count('dialog choice callback complete')
                        ready_after = len(lines())
                        pulse(1)
                        wait(lambda text: text.count('dialog choice callback complete') > callbacks or
                             'screen dispatch id=4 ' in text[at:], 'original dialog choice callback')
                        choices_taken += 1
                    wait(lambda text: 'screen dispatch id=4 ' in text[at:] and
                         'screen Run first frame attempted' in text[text.rfind('screen dispatch id=4 '):],
                         'real hangar after character introduction')
                    report['native_dialog_choices'] = choices_taken
                    report['hangar_opened'] = True
                    select('ButTakeOff')
                    at = len(lines())
                    pulse(1)
                    wait(lambda text: 'screen dispatch id=16 ' in text[at:] and
                         'HOST map orders ready' in text[at:], 'real takeoff and star map', 90)
                    report.update(status='PASS', map_orders_ready=True,
                                  scope='raw pad campaign through Unicode OSK, generation, character choices, hangar and takeoff to map')
                    if args.save:
                        report['status'] = 'RUNNING'
                        select('PM_Logo')
                        at = len(lines())
                        pulse(1)
                        wait(lambda text: 'screen dispatch id=25 ' in text[at:] and
                             'screen Run first frame attempted' in text[text.rfind('screen dispatch id=25 '):],
                             'original in-game menu')
                        select('Save')
                        at = len(lines())
                        pulse(1)
                        wait(lambda text: 'screen dispatch id=23 ' in text[at:] and
                             any(re.fullmatch(r'Slot\d+Edit', node[2]) for node in graph()[2]),
                             'original manual save UI')
                        editable = [node for node in graph()[2] if re.fullmatch(r'Slot\d+Edit', node[2])]
                        select(min(editable, key=lambda node: node[1][1])[2])
                        pulse(1)
                        wait(lambda text: 'HOST OSK fixture call=3 result=Accept' in text, 'Unicode save name')
                        pulse(1)
                        wait(lambda text: 'HOST OSK fixture call=4 initial_matches=1 result=Cancel' in text,
                             'save name retained on OSK cancel')
                        select('ButSave')
                        before = set((config / 'save').glob('*.sav'))
                        pulse(1)
                        expected_save = config / 'save/Проба Ёж.sav'
                        wait(lambda text: manual_committed() and expected_save.exists() and expected_save.stat().st_size > 64,
                             'physical Unicode save after takeoff', 90)
                        if expected_save in before:
                            raise RuntimeError('Fixture save unexpectedly existed before the write')
                        report.update(status='PASS', manual_save_path=str(expected_save),
                                      manual_save_bytes=expected_save.stat().st_size,
                                      manual_save_sha256=hashlib.sha256(expected_save.read_bytes()).hexdigest(),
                                      new_process_reload_verified=False,
                                      scope='raw pad campaign and original Unicode manual save after takeoff; reload pending')
            else:
                select('Load')
                at = len(lines())
                pulse(1)
                wait(lambda text: 'screen dispatch id=23 ' in text[at:] and
                     any(node[2] == 'Slot0' for node in graph()[2]), 'load UI copied slot')
                select('Slot0')
                pulse(1)
                select('ButLoad')
                at = len(lines())
                pulse(1)
                wait(lambda text: 'screen dispatch id=24 ' in text[at:] and
                     'screen dispatch id=16 ' in text[at:] and
                     'HOST map orders ready' in text[at:], 'fresh-process original load and map', 90)
                report.update(status='PASS', new_process_reload_verified=True,
                              loaded_copied_file=source_save.name, checkpoint_comparison_verified=False,
                              scope='raw pad original load UI, copied Unicode save, fresh process and map orders mode')
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
    if source_save:
        report['source_save_unchanged'] = hashlib.sha256(source_save.read_bytes()).hexdigest() == source_digest
        if not report['source_save_unchanged']:
            report.update(status='FAIL', error='Input save fixture changed')
    result_path.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2), flush=True)
    if report['status'] != 'PASS':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
