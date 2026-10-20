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
    parser.add_argument('--fail-save-once', action='store_true', help='Refuse the first UI save with a private occupied temp directory, then retry')
    parser.add_argument('--load-save', type=Path, help='Load a copied build fixture in a separate fresh process')
    parser.add_argument('--advance-turns', type=int, default=0)
    parser.add_argument('--resave-name', help='After loading, write a new slot using host OSK')
    parser.add_argument('--expect-checkpoint', type=Path, help='Compare the loaded state to a prior route result')
    parser.add_argument('--label', help='Separate result folder under build/e2e515')
    parser.add_argument('--scene-cycles', type=int, default=0, help='After campaign takeoff, repeat real ship/menu/modal/return transitions with requested frame captures')
    parser.add_argument('--planet-cycles', type=int, default=0, help='After loading a copied docked save, visit equipment/goods/info/hangar/planet via real pad actions')
    parser.add_argument('--trade-dialog', action='store_true', help='Open a real market quantity dialog from native focus, hold A, cancel and verify ownership')
    parser.add_argument('--trade-roundtrip', action='store_true', help='After Cancel, buy and sell one unit through the original quantity UI on a copied save')
    args = parser.parse_args()
    if args.trade_roundtrip:
        args.trade_dialog = True
    if not 0 <= args.scene_cycles <= 10 or (args.scene_cycles and not args.campaign):
        parser.error('--scene-cycles requires --campaign and a count between 1 and 10')
    if not 0 <= args.planet_cycles <= 10 or (args.planet_cycles and not args.load_save):
        parser.error('--planet-cycles requires --load-save and a count between 1 and 10')
    if args.trade_dialog and not args.load_save:
        parser.error('--trade-dialog requires --load-save')
    if args.save and not args.campaign:
        parser.error('--save requires --campaign')
    if args.fail_save_once and not (args.save or args.resave_name):
        parser.error('--fail-save-once requires a save route')
    if args.load_save and (args.campaign or args.save):
        parser.error('--load-save is a separate fresh-process route')
    if (args.advance_turns or args.resave_name or args.expect_checkpoint) and not args.load_save:
        parser.error('turns/resave/checkpoint require --load-save')
    if not 0 <= args.advance_turns <= 10:
        parser.error('--advance-turns must be between 0 and 10')
    if args.resave_name and (any(c in args.resave_name for c in '<>"/\\:') or
                            not 1 <= len(args.resave_name.encode('utf-16-le')) // 2 <= 55):
        parser.error('invalid fixture save name')
    if args.label and not re.fullmatch(r'[a-zA-Z0-9_-]{1,40}', args.label):
        parser.error('invalid result folder label')
    global OUTPUT
    if args.campaign:
        OUTPUT = ROOT / 'build/e2e515/controller-campaign'
    if args.save:
        OUTPUT = ROOT / 'build/e2e515/controller-save'
    if args.load_save:
        OUTPUT = ROOT / 'build/e2e515/controller-reload'
    if args.label:
        OUTPUT = ROOT / 'build/e2e515' / args.label
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
    if args.scene_cycles or args.planet_cycles or args.trade_dialog or args.expect_checkpoint:
        env['E2E515_HOST_FRAME_CAPTURE'] = '1'
    if args.resave_name:
        env.update(E2E515_HOST_ROUTE_OSK='1', E2E515_HOST_SAVE_NAME=args.resave_name)
    if args.save or args.load_save:
        env['E2E515_SAVE_CHECKPOINTS'] = '1'
    log = stage / 'sdmc:/switch/space-rangers-hd-a-war-apart/logs/e2e.log'
    game_log = config / '########.log'
    sequence = 0
    last_log = ''
    report = {'status': 'RUNNING', 'stage': str(stage), 'hardware_executed': False,
              'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
              'normal_shutdown_verified': False}
    if source_save:
        report.update(source_save_path=str(source_save), source_save_sha256=source_digest)
    result_path = stage / 'result.json'

    def publish_report():
        content = json.dumps(report, indent=2) + '\n'
        result_path.write_text(content)
        (OUTPUT / 'result.json').write_text(content)

    publish_report()
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

        def checkpoints():
            try:
                text = (stage / 'stderr.log').read_text(errors='replace')
            except OSError as error:
                if error.errno != errno.ENODATA:
                    raise
                return []
            return [json.loads(line.split('|', 1)[1]) for line in text.splitlines()
                    if line.startswith('E2E515_CHECKPOINT|') and line.endswith('}')]

        def restored_screen(text):
            records = list(re.finditer(r'screen dispatch id=(\d+) postload=', text))
            if not records or int(records[-1][1]) not in (4, 5, 6, 16, 34):
                return 0
            tail = text[records[-1].end():]
            ready = 'HOST map orders ready' if int(records[-1][1]) == 16 else 'screen Run first frame attempted'
            return int(records[-1][1]) if ready in tail else 0

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

        def capture(screen, label):
            samples = report.setdefault('scene_frames', [])
            sequence = len(samples) + 1
            temporary = frames / 'frame-request.tmp'
            temporary.write_text(f'{sequence} {screen}\n')
            temporary.replace(frames / 'frame-request.txt')
            metadata = frames / f'frame-{sequence}.json'
            wait(lambda text: metadata.exists(), 'presented frame capture ' + label)
            data = json.loads(metadata.read_text())
            if not data['complete'] or data['screen'] != screen:
                raise RuntimeError('Requested frame capture failed: ' + label)
            samples.append(dict(label=label, **data))
            shutil.copyfile(stage / 'focus.txt', frames / f'frame-{sequence}.focus.txt')

        def select(name, remaining=128):
            if remaining <= 0:
                raise RuntimeError('Focus route did not converge: ' + name)
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
            for key, reached in path[:1]:
                # One graph edge per press. Do not intentionally hold navigation
                # while waiting on diagnostic log I/O: that permits real repeats.
                pulse(key, 0)
                wait(lambda text: state()[:2] == (scope, reached) or
                     (graph()[0] == scope and graph()[1] != selected and graph()[2] != nodes),
                     'actual focus step to ' + name)
                # Opening panels and hover sequences can move/resize real hit
                # boxes. Replan from the newly published graph after every edge.
                return select(name, remaining - 1)

        def save_world(name, accept, cancel):
            select('PM_Logo')
            at = len(lines()); pulse(1)
            wait(lambda text: 'screen dispatch id=25 ' in text[at:] and
                 'screen Run first frame attempted' in text[text.rfind('screen dispatch id=25 '):],
                 'original in-game menu')
            select('Save')
            at = len(lines()); pulse(1)
            wait(lambda text: 'screen dispatch id=23 ' in text[at:] and
                 any(re.fullmatch(r'Slot\d+Edit', node[2]) for node in graph()[2]), 'original manual save UI')
            editable = [node for node in graph()[2] if re.fullmatch(r'Slot\d+Edit', node[2])]
            select(min(editable, key=lambda node: node[1][1])[2])
            pulse(1); wait(lambda text: accept in text, 'Unicode save name')
            pulse(1); wait(lambda text: cancel in text, 'save name retained on OSK cancel')
            select('ButSave')
            before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (config / 'save').glob('*.sav')}
            if args.fail_save_once:
                parent_scope = state()[0]
                occupied_temp = config / 'save/save.tmp'
                # This fresh run owns the empty directory. Never replace any file.
                occupied_temp.mkdir()
                pulse(1)
                wait(lambda text: state()[0] != parent_scope and state()[2] > 0 and
                     game_log.exists() and 'occupied temp is not a readable regular file' in
                     game_log.read_text(errors='replace'), 'original save error dialog', 90)
                if manual_committed():
                    raise RuntimeError('Failed save was reported as committed')
                failed_slots = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in (config / 'save').glob('*.sav')}
                if failed_slots != before:
                    raise RuntimeError('Failed save changed existing slots')
                occupied_temp.rmdir()
                pulse(1)
                wait(lambda text: state()[0] == parent_scope and
                     any(node[2] == 'ButSave' for node in graph()[2]),
                     'error dismissed back to active save UI')
                select('ButSave')
                report['save_failure_ui_retry'] = True
            pulse(1)
            expected_save = config / 'save' / (name + '.sav')
            wait(lambda text: manual_committed() and expected_save.exists() and expected_save.stat().st_size > 64,
                 'physical manual save committed', 90)
            if expected_save.name in before:
                raise RuntimeError('Fixture destination existed before unique-slot write')
            after = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (config / 'save').glob('*.sav')}
            preserved = all(after.get(name) == digest for name, digest in before.items())
            if not preserved:
                raise RuntimeError('Manual save changed another existing slot')
            saved = [row for row in checkpoints() if row['phase'] == 'save']
            if not saved:
                raise RuntimeError('Missing real serialized-world checkpoint')
            return dict(manual_save_path=str(expected_save), manual_save_bytes=expected_save.stat().st_size,
                        manual_save_sha256=hashlib.sha256(expected_save.read_bytes()).hexdigest(),
                        original_slots_preserved=preserved, saved_checkpoint=saved[-1], new_process_reload_verified=False)

        try:
            wait(lambda text: 'screen first presented id=1 ' in text and state()[2] > 0,
                 'main menu and native focus graph', 180)
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
                    if args.scene_cycles: capture(3, 'generation-ready')
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
                        if args.scene_cycles and not choices_taken: capture(20, 'character-dialog')
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
                    if args.scene_cycles: capture(4, 'hangar')
                    select('ButTakeOff')
                    at = len(lines())
                    pulse(1)
                    wait(lambda text: 'screen dispatch id=16 ' in text[at:] and
                         'HOST map orders ready' in text[at:], 'real takeoff and star map', 90)
                    report.update(status='PASS', map_orders_ready=True,
                                  scope='raw pad campaign through Unicode OSK, generation, character choices, hangar and takeoff to map')
                    if args.scene_cycles:
                        report['status'] = 'RUNNING'
                        capture(16, 'map-initial')
                        for cycle in range(1, args.scene_cycles + 1):
                            select('PM_Ship'); at = len(lines()); pulse(1)
                            wait(lambda text: 'screen dispatch id=10 ' in text[at:] and
                                 'screen Run first frame attempted' in text[text.rfind('screen dispatch id=10 '):], 'ship overlay')
                            capture(10, f'cycle-{cycle}-ship')
                            select('Exit'); at = len(lines()); pulse(1)
                            wait(lambda text: 'screen dispatch id=16 ' in text[at:] and
                                 'HOST map orders ready' in text[at:], 'ship returns to map', 90)
                            capture(16, f'cycle-{cycle}-map-after-ship')
                            select('PM_Logo'); at = len(lines()); pulse(1)
                            wait(lambda text: 'screen dispatch id=25 ' in text[at:] and
                                 'screen Run first frame attempted' in text[text.rfind('screen dispatch id=25 '):], 'game menu')
                            select('Exit'); capture(25, f'cycle-{cycle}-menu-before-modal')
                            parent = state()[0]; pulse(1)
                            wait(lambda text: state()[0] != parent and state()[2] > 0, 'real exit confirmation')
                            capture(25, f'cycle-{cycle}-exit-confirmation')
                            pulse(2)
                            wait(lambda text: state()[0] == parent, 'cancel exit returns to same menu owner')
                            capture(25, f'cycle-{cycle}-menu-after-modal')
                            select('Resume'); at = len(lines()); pulse(1)
                            wait(lambda text: 'screen dispatch id=16 ' in text[at:] and
                                 'HOST map orders ready' in text[at:], 'menu returns to map', 90)
                            capture(16, f'cycle-{cycle}-map-after-menu')
                        report.update(status='PASS', scene_cycles=args.scene_cycles,
                                      scope='real campaign and repeated raw-pad ship/menu/exit-confirmation cancellation; frame analysis pending')
                    if args.save:
                        report['status'] = 'RUNNING'
                        report.update(save_world('Проба Ёж',
                            'HOST OSK fixture call=3 result=Accept',
                            'HOST OSK fixture call=4 initial_matches=1 result=Cancel'))
                        report.update(status='PASS', scope='raw pad campaign and original Unicode manual save after takeoff; reload pending')
            else:
                select('Load')
                at = len(lines())
                pulse(1)
                wait(lambda text: 'screen dispatch id=23 ' in text[at:] and
                     any(node[2] == 'Slot0' for node in graph()[2]), 'load UI copied slot')
                if args.planet_cycles: capture(23, 'load-manager')
                select('Slot0')
                pulse(1)
                select('ButLoad')
                at = len(lines())
                pulse(1)
                wait(lambda text: 'screen dispatch id=24 ' in text[at:] and restored_screen(text),
                     'fresh-process original load and gameplay', 90)
                wait(lambda text: any(row['phase'] == 'load' for row in checkpoints()), 'loaded world checkpoint')
                loaded = next(row for row in checkpoints() if row['phase'] == 'load')
                report.update(status='PASS', new_process_reload_verified=True,
                              loaded_copied_file=source_save.name, checkpoint_comparison_verified=False,
                              loaded_checkpoint=loaded, restored_screen=restored_screen(lines()),
                              scope='raw pad original load UI, copied save and fresh process')
                if args.expect_checkpoint:
                    expected = json.loads(args.expect_checkpoint.read_text())
                    if expected['manual_save_sha256'] != source_digest:
                        raise RuntimeError('Expected checkpoint belongs to a different save file')
                    previous = expected['saved_checkpoint']
                    diff = {key: [previous.get(key), value] for key, value in loaded.items()
                            if key != 'phase' and previous.get(key) != value}
                    report['checkpoint_diff'] = diff
                    if diff:
                        raise RuntimeError('Loaded world checkpoint differs from saved state')
                    report['checkpoint_comparison_verified'] = True
                    capture(report['restored_screen'], 'restored-save-screen')
                if args.planet_cycles:
                    if report['restored_screen'] != 5:
                        raise RuntimeError('Planet tour requires an original inhabited-planet save')
                    report['status'] = 'RUNNING'
                    capture(5, 'planet-initial')
                    trace = []
                    route = [('PP_Shop', 8, 'equipment'), ('PP_Goods', 34, 'goods'),
                             ('PP_Info', 28, 'info'), ('PP_Hangar', 4, 'hangar'),
                             ('ButClose', 5, 'planet')]
                    for cycle in range(1, args.planet_cycles + 1):
                        for button, screen, name in route:
                            select(button); parent = state()[0]; pulse(1)
                            wait(lambda text, parent=parent: state()[0] != parent and state()[2] > 0,
                                 'planet panel transition to ' + name, 90)
                            # Re-entry does not always emit the dispatch marker.
                            # The requested present below verifies the real top
                            # registered owner, not an inferred log substring.
                            capture(screen, f'planet-cycle-{cycle}-{name}')
                            trace.append(dict(cycle=cycle, button=button, screen=screen))
                    report.update(status='PASS', planet_cycles=args.planet_cycles, planet_transitions=trace,
                                  scope='copied real planet save, raw-pad equipment/goods/info/hangar/planet transitions; image analysis pending')
                if args.trade_dialog:
                    if report['restored_screen'] != 5:
                        raise RuntimeError('Trade dialog probe requires an original inhabited-planet save')
                    report['status'] = 'RUNNING'
                    select('PP_Goods'); at = len(lines()); pulse(1)
                    wait(lambda text: 'screen dispatch id=34 ' in text[at:] and
                         'screen Run first frame attempted' in text[text.rfind('screen dispatch id=34 '):], 'real goods shop')
                    capture(34, 'goods-before-quantity')
                    available = [node[2] for node in graph()[2] if re.fullmatch(r'Tov[0-7]', node[2])]
                    if not available:
                        raise RuntimeError('Real market rows are absent from native focus graph')
                    select(sorted(available)[0]); parent = state()[0]; at = len(lines())
                    pulse(1, .9)
                    wait(lambda text: state()[0] != parent and
                         {'Close', 'Ok'}.issubset({node[2] for node in graph()[2]}),
                         'original quantity dialog; held A must not accept it')
                    if next(node[2] for node in graph()[2] if node[0] == state()[1]) != 'Ok':
                        raise RuntimeError('Quantity dialog did not focus its original acceptance button')
                    confirms = lines()[at:].count('controller native action=Confirm ')
                    if confirms != 1:
                        raise RuntimeError(f'Expected one goods Confirm across dialog change, observed {confirms}')
                    report['trade_native_confirms'] = confirms
                    capture(34, 'goods-quantity-dialog')
                    pulse(2); wait(lambda text: state()[0] == parent, 'quantity Cancel restores same goods owner')
                    capture(34, 'goods-after-cancel')
                    wait(lambda text: any(row['phase'] == 'trade-quantity-after' for row in checkpoints()),
                         'trade Cancel world checkpoint')
                    before = next(row for row in checkpoints() if row['phase'] == 'trade-quantity-before')
                    after = next(row for row in checkpoints() if row['phase'] == 'trade-quantity-after')
                    differences = {key: [before.get(key), value] for key, value in after.items()
                                   if key != 'phase' and before.get(key) != value}
                    report['trade_checkpoint_diff'] = differences
                    if differences:
                        raise RuntimeError('Quantity Cancel changed the recorded game state')
                    report.update(status='PASS', trade_quantity_cancel=True,
                                  scope='raw pad market row opens original quantity dialog, held A suppressed, B cancels')
                    if args.trade_roundtrip:
                        def trades():
                            return [json.loads(line.split('|', 1)[1]) for line in
                                    (stage / 'stderr.log').read_text(errors='replace').splitlines()
                                    if line.startswith('E2E515_TRADE|') and line.endswith('}')]

                        def quantity():
                            matches = list(re.finditer(r'controller quantity value=(\d+) minimum=(\d+) maximum=(\d+) limit=(\d+)', lines()))
                            return tuple(map(int, matches[-1].groups())) if matches else None

                        market_row = sorted(available)[0]
                        operations = []
                        for operation, row_name in [('buy', market_row), ('sell', 'Tov' + str(10 + int(market_row[3:])) )]:
                            select(row_name); parent = state()[0]; records = len(trades())
                            pulse(1)
                            wait(lambda text: state()[0] != parent and
                                 {'Close', 'Ok'}.issubset({node[2] for node in graph()[2]}), operation + ' quantity dialog')
                            raw(32, 0)
                            wait(lambda text: quantity() is not None and quantity()[0] == quantity()[2],
                                 operation + ' quantity reaches original maximum', 40)
                            raw(0)
                            pulse(32, 0)
                            if quantity()[0] != quantity()[2]:
                                raise RuntimeError('Quantity moved above its original maximum')
                            # Exercise the original minimum clamp via real repeated pad input.
                            raw(16, 0)
                            wait(lambda text: quantity() is not None and quantity()[0] == quantity()[1] == 0,
                                 operation + ' quantity reaches original minimum', 40)
                            raw(0)
                            pulse(16, 0)
                            if quantity()[0] != 0:
                                raise RuntimeError('Quantity moved below its original minimum')
                            pulse(32, 0)
                            if quantity()[0] != 1:
                                raise RuntimeError('Right pad step did not select exactly one unit')
                            capture(34, operation + '-one-unit')
                            pulse(1)
                            wait(lambda text: state()[0] == parent and len(trades()) >= records + 2,
                                 operation + ' transaction completes in original shop', 60)
                            before, after = trades()[records:records + 2]
                            changed = [(old, new) for old, new in zip(before['goods'], after['goods']) if old != new]
                            cargo_changes = [(old, new) for old, new in changed if old['cargo'] != new['cargo']]
                            if len(cargo_changes) != 1:
                                raise RuntimeError(operation + ' did not change exactly one cargo good')
                            old, new = cargo_changes[0]
                            direction = 1 if operation == 'buy' else -1
                            price = old[operation]
                            if (new['cargo'] != old['cargo'] + direction or
                                    new['stock'] != old['stock'] - direction or
                                    after['money'] != before['money'] - direction * price or
                                    after['free_space'] != before['free_space'] - direction):
                                raise RuntimeError(operation + ' money/cargo/stock/free-space conservation failed')
                            if any(a != b for a, b in zip(before['goods'], after['goods']) if a['id'] != old['id']):
                                raise RuntimeError(operation + ' changed unrelated goods')
                            if operations and old['id'] != operations[0]['good']:
                                raise RuntimeError('Sale selected a different good after purchase')
                            operations.append(dict(operation=operation, good=old['id'], quantity=1,
                                                   price=price, minimum_clamp_verified=True,
                                                   maximum_clamp_verified=True, before=before, after=after))
                            capture(34, operation + '-completed')
                        report.update(status='PASS', trade_roundtrip=operations,
                                      scope='copied save, original one-unit purchase and sale, numeric conservation; hardware pending')
                if args.advance_turns:
                    if report['restored_screen'] not in (4, 5, 6):
                        raise RuntimeError('This turn scenario requires a recovered docked/planet state')
                    for turn in range(loaded['turn'] + 1, loaded['turn'] + args.advance_turns + 1):
                        callbacks = lines().count('turn operation=end-turn-ui')
                        select('PM_EndTurn'); pulse(1)
                        wait(lambda text: f'hangar_end_turn_done turn={turn}' in text or
                             text.count('turn operation=end-turn-ui') > callbacks,
                             f'original end-turn handler reaches {turn}', 90)
                    report['turns_advanced'] = args.advance_turns
                if args.resave_name:
                    report.update(save_world(args.resave_name,
                        'HOST OSK save fixture call=1 result=Accept',
                        'HOST OSK save fixture call=2 initial_matches=1 result=Cancel'))
                    if report['saved_checkpoint']['turn'] != loaded['turn'] + args.advance_turns:
                        raise RuntimeError('Saved turn does not match the requested actual turn advance')
                    report.update(status='PASS', scope='recovered real state, native end-turns and original manual resave UI')
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
    publish_report()
    print(json.dumps(report, indent=2), flush=True)
    if report['status'] != 'PASS':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
