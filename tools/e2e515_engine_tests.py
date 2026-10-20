"""Link focused tests to the complete effective host game without editing upstream."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'port/switch/build/e2e5-host-game/optimized'
OUTPUT = ROOT / 'build/e2e515/engine-tests'


def run(command, name, cwd=ROOT):
    result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, timeout=1800)
    (OUTPUT / f'{name}.log').write_text(result.stdout)
    if result.returncode:
        print(result.stdout[-6000:], flush=True)
        raise RuntimeError(f'{name}: exit {result.returncode}')
    return result.stdout


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    report = OUTPUT / 'result.json'
    report.write_text(json.dumps({'status': 'RUNNING'}))
    run(['python3', 'tools/e2e5_host_game_build.py', '--profile', 'optimized', '--jobs', '4'], 'host-build')
    build_report = json.loads((BUILD / 'report.json').read_text())
    effective_loop = ROOT / build_report['source_map']['upstream/cpp/src/GI_MessageLoop.cpp']
    effective_arcade = ROOT / build_report['source_map']['upstream/cpp/src/ab_MainForm.cpp']
    effective_edit = ROOT / build_report['source_map']['upstream/cpp/src/GI_Edit.cpp']
    if effective_loop.read_text().count('InputContextScope e2e_input_owner(') != 2:
        raise RuntimeError('Scoped input owner missing from effective Run/RunContinuous')
    if effective_loop.read_text().count('e2e_input_owner.CancelHeldInput();') != 2:
        raise RuntimeError('Input must be cancelled before both effective OnClose calls')
    if effective_loop.read_text().count('MessageLoopStackGuard e2e_loop_stack(this);') != 2:
        raise RuntimeError('Both real loops must unwind their original MessageLoopStack')
    if 'SetArcadeControls(' in effective_arcade.read_text():
        raise RuntimeError('Effective OnOpen/OnClose overrides scoped owner')
    if 'controller_text_entry::Open(this);' not in effective_edit.read_text():
        raise RuntimeError('Generic keyboard missing from the real TEditGI input path')
    flags = build_report['compile_flags']
    original = [Path(name) for name in (BUILD / 'link-objects.rsp').read_text().splitlines()
                if '/objects/host/' not in name]
    results = []
    try:
        with tempfile.TemporaryDirectory(prefix='e2e515-host-checks-') as temp:
            staging = Path(temp)
            objects = []
            for i, source in enumerate(original):
                target = staging / f'{i}.o'
                shutil.copyfile(source, target)
                objects.append(str(target))
            response = staging / 'objects.rsp'
            response.write_text('\n'.join(objects) + '\n')
            runtime = staging / 'runtime-implementation.o'
            run(['g++', *flags, '-c', str(ROOT / 'port/switch/platform/runtime_loop_slice.cpp'),
                 '-o', str(runtime)], 'runtime-compile')
            library = BUILD / 'okgf/libokgf.so'
            for name, test, extras in (
                    ('runtime-loop', 'test_runtime_loop_slice.cpp', [str(runtime)]),
                    ('scene-canvas', 'test_e2e513_scene_canvas.cpp', []),
                    ('screen-context', 'test_e2e515_screen_context.cpp', [])):
                source = ROOT / 'tests' / test
                obj = staging / f'{name}.o'
                run(['g++', *flags, '-c', str(source), '-o', str(obj)], name + '-compile')
                binary = staging / name
                run(['g++', '@' + str(response), str(obj), *extras, '-Wl,--gc-sections',
                     str(library), '-Wl,-rpath,' + str(library.parent), '-lz', '-lpng', '-ljpeg',
                     '-lvorbisfile', '-lvorbis', '-logg', '-lSDL2', '-lm', '-pthread',
                     '-o', str(binary)], name + '-link')
                output = run([str(binary)], name + '-run', cwd=staging)
                results.append({'name': name, 'status': 'PASS', 'stdout': output,
                                'test_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                                'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest()})
                print(f'{name}: PASS', flush=True)
    except Exception as error:
        report.write_text(json.dumps({'status': 'FAIL', 'error': str(error), 'tests': results}, indent=2))
        raise
    report.write_text(json.dumps({'status': 'PASS', 'host_build': build_report['binary_sha256'],
                                 'effective_loop_sha256': hashlib.sha256(effective_loop.read_bytes()).hexdigest(),
                                 'scoped_input_effective_sites': 2,
                                 'tests': results}, indent=2) + '\n')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        report = OUTPUT / 'result.json'
        if report.exists() and json.loads(report.read_text()).get('status') == 'RUNNING':
            report.write_text(json.dumps({'status': 'FAIL', 'error': str(error)}, indent=2) + '\n')
        raise
