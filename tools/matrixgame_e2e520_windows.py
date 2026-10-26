"""Address-build the full original Windows candidate from recorded E2E-5.19 inputs.

Reuses hashed archives, never touches production artifacts or upstream resources.
The runtime exit code is recorded verbatim; only zero is a completed run.
"""
import json
import os
from pathlib import Path
import subprocess
import shutil
import argparse
from matrixgame_engine_prepare import prepare_engine

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--stage',choices=('e2e520','e2e521'),default='e2e520')
    stage=parser.parse_args().stage
    old = ROOT / 'build/matrixgame/e2e519'
    out = ROOT / ('build/matrixgame/'+stage+'/windows')
    source = ROOT / ('build/matrixgame/'+stage+'/source')
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'result.json').is_file():
        attempts=out/'attempts'
        attempts.mkdir(exist_ok=True)
        attempt=attempts/str(len(list(attempts.iterdir()))+1).zfill(3)
        attempt.mkdir()
        for name in ('result.json','runtime.log','windows-game.exe','windows-game.map'):
            if (out/name).is_file():shutil.copy2(out/name,attempt/name)
    prepare_engine(source)
    prior = json.loads((old / 'skin-borrow-windows/provider-result.json').read_text())
    base = prior['commands']['sdk-main']
    results = {}
    commands = {}
    env = dict(os.environ, TEMP=str(old / 'temp'), TMP=str(old / 'temp'))
    report = {'commands': commands, 'results': results, 'runtime_exit': None}
    def save():
        (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    def run(command, label, cwd=ROOT):
        commands[label] = command
        script = out / (label + '.cmd')
        script.write_text(command + '\n')
        value = subprocess.run(['cmd.exe', '/d', '/c', str(script)], cwd=cwd,
                               env=env, capture_output=True, text=True)
        results[label] = value.returncode
        (out / (label + '.log')).write_text(value.stdout + value.stderr)
        print(label, value.returncode, (value.stdout + value.stderr)[-4000:] if value.returncode else '', flush=True)
        save()
        if value.returncode:
            raise SystemExit(value.returncode)
    units = {
        'sdk-main': ROOT / 'tests/matrixgame_windows_frame_main.cpp',
        'engine-main': ROOT / 'tests/matrixgame_windows_frame_init.cpp',
        'platform': ROOT / 'port/matrixgame/native-engine/platform_win32.cpp',
        'map-device': ROOT / 'port/matrixgame/native-engine/map_device_backend.cpp',
        'owner': ROOT / 'port/matrixgame/native-engine/original_game.cpp',
        'prepare': source / 'MatrixGame/src/OriginalMapPrepare.cpp',
        'map': source / 'MatrixGame/src/MatrixMap.cpp',
        'frame': source / 'MatrixGame/src/OriginalWindowsFrame.cpp',
        'game': source / 'MatrixGame/src/MatrixGame.cpp',
        'init': ROOT / 'port/matrixgame/native-engine/init_runtime.cpp',
        'platform3g': ROOT / 'port/matrixgame/native-engine/platform_3g.cpp',
    }
    for label, path in units.items():
        command = base.replace(str(ROOT / 'tests/matrixgame_windows_game_main.cpp'), str(path))
        command = command.replace(str(old / 'skin-borrow-source'), str(source))
        command = command.replace(str(old / 'skin-borrow-windows/sdk-main.obj'), str(out / (label + '.obj')))
        run(command, label)
    command = prior['commands']['link']
    for label in units:
        command = command.replace(str(old / ('skin-borrow-windows/' + label + '.obj')), str(out / (label + '.obj')))
    command += ' "' + str(out / 'map-device.obj') + '"'
    command += ' "' + str(out / 'frame.obj') + '"'
    command += ' "' + str(out / 'init.obj') + '" "' + str(out / 'platform3g.obj') + '"'
    resource=base[:base.index(' && cl ')] if ' && cl ' in base else base[:base.index('&& cl ')]
    resource+=' && rc /nologo /I"'+str(ROOT/'upstream/matrixgame/MatrixGame')+'" /fo"'+str(out/'frame.res')+'" "'+str(ROOT/'tests/matrixgame_windows_frame.rc')+'"'
    run(resource,'resource')
    command+=' "'+str(out/'frame.res')+'"'
    command = command.replace(str(old / 'skin-borrow-windows/windows-game.exe'), str(out / 'windows-game.exe'))
    command = command.replace(str(old / 'skin-borrow-windows/windows-game.map'), str(out / 'windows-game.map'))
    run(command, 'link', old / 'original-init-windows')
    # The existing runtime directory contains the read-only package hardlink and
    # CFG copies used in E2E-5.19; no resource copy or edits are needed.
    cwd = old / 'skin-borrow-windows/runtime'
    command = [str(out / 'windows-game.exe'), str(out)]
    commands['runtime'] = command
    value = subprocess.run(command, cwd=cwd, env=env, capture_output=True, text=True, timeout=180)
    report.update(runtime_exit=value.returncode, stdout=value.stdout, stderr=value.stderr, cwd=str(cwd))
    (out / 'runtime.log').write_text(value.stdout + value.stderr)
    save()
    print('runtime', value.returncode, value.stdout, value.stderr, flush=True)


if __name__ == '__main__':
    main()
