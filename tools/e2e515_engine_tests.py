"""Link focused tests to the complete effective host game without editing upstream."""
import argparse
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize-scene', action='store_true', help='Instrument renderer, graph buffers, GR_Main, GI loop and the scene lifecycle test; other game objects remain uninstrumented')
    parser.add_argument('--sanitize-item-references', action='store_true', help='Instrument actual galaxy/save-reference implementation and its ownership/typed-ID test; other game units remain uninstrumented')
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    report = OUTPUT / 'result.json'
    report.write_text(json.dumps({'status': 'RUNNING'}))
    run(['python3', 'tools/e2e5_host_game_build.py', '--profile', 'optimized', '--jobs', '4'], 'host-build')
    build_report = json.loads((BUILD / 'report.json').read_text())
    effective_loop = ROOT / build_report['source_map']['upstream/cpp/src/GI_MessageLoop.cpp']
    effective_arcade = ROOT / build_report['source_map']['upstream/cpp/src/ab_MainForm.cpp']
    effective_edit = ROOT / build_report['source_map']['upstream/cpp/src/GI_Edit.cpp']
    effective_galaxy = ROOT / build_report['source_map']['upstream/cpp/src/aGalaxy.cpp']
    for hook in ('e2e515_save_checkpoint::Observe(this, "save");',
                 'e2e515_save_checkpoint::Observe(Self, "load");'):
        if effective_galaxy.read_text().count(hook) != 1:
            raise RuntimeError('Real save/load checkpoint missing from effective galaxy: ' + hook)
    if effective_loop.read_text().count('InputContextScope e2e_input_owner(') != 2:
        raise RuntimeError('Scoped input owner missing from effective Run/RunContinuous')
    if effective_loop.read_text().count('e2e_input_owner.CancelHeldInput();') != 2:
        raise RuntimeError('Input must be cancelled before both effective OnClose calls')
    if effective_loop.read_text().count('MessageLoopStackGuard e2e_loop_stack(this);') != 2:
        raise RuntimeError('Both real loops must unwind their original MessageLoopStack')
    for hook, expected in (('controller_ui::Scope e2e_controller_ui(this);', 2),
                           ('controller_ui::Process(this, Message, WParam, LParam)', 1),
                           ('controller_ui::Draw(this);', 1)):
        if effective_loop.read_text().count(hook) != expected:
            raise RuntimeError(f'Native focus hook missing or duplicated: {hook}')
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
                    ('screen-context', 'test_e2e515_screen_context.cpp', []),
                    ('native-focus', 'test_e2e515_native_focus.cpp', []),
                    ('item-references', 'test_e2e515_item_references.cpp', [])):
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
            if args.sanitize_scene:
                sanitizer_flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
                selected = {'renderer_platform': ROOT / 'port/switch/platform/renderer_platform.cpp'}
                for unit in ('GR_Main', 'GR_GraphBuf', 'GI_MessageLoop'):
                    selected[unit] = ROOT / build_report['source_map'][f'upstream/cpp/src/{unit}.cpp']
                replacements = {}
                for unit, source in selected.items():
                    obj = staging / ('sanitized-' + unit + '.o')
                    run(['g++', *flags, *sanitizer_flags, '-c', str(source), '-o', str(obj)], 'sanitized-' + unit)
                    replacements[unit] = obj
                response = staging / 'sanitized-objects.rsp'
                response.write_text('\n'.join(str(replacements.get(source.stem, objects[i]))
                                             for i, source in enumerate(original)) + '\n')
                source = ROOT / 'tests/test_e2e513_scene_canvas.cpp'
                obj = staging / 'sanitized-test.o'
                run(['g++', *flags, *sanitizer_flags, '-c', str(source), '-o', str(obj)], 'sanitized-scene-compile')
                binary = staging / 'sanitized-scene'
                run(['g++', *sanitizer_flags, '@' + str(response), str(obj), '-Wl,--gc-sections',
                     str(library), '-Wl,-rpath,' + str(library.parent), '-lz', '-lpng', '-ljpeg',
                     '-lvorbisfile', '-lvorbis', '-logg', '-lSDL2', '-lm', '-pthread', '-o', str(binary)], 'sanitized-scene-link')
                output = run(['env', 'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1',
                              'UBSAN_OPTIONS=halt_on_error=1', str(binary)], 'sanitized-scene-run', cwd=staging)
                results.append(dict(name='sanitized-scene', status='PASS', stdout=output,
                                    instrumented_units=list(selected), remaining_game_units_instrumented=False,
                                    leak_detection=True, binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest()))
                print('sanitized-scene: PASS', flush=True)
            if args.sanitize_item_references:
                sanitizer_flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
                galaxy_obj = staging / 'sanitized-galaxy.o'
                run(['g++', *flags, *sanitizer_flags, '-c', str(effective_galaxy), '-o', str(galaxy_obj)],
                    'sanitized-item-galaxy')
                response = staging / 'item-objects.rsp'
                response.write_text('\n'.join(str(galaxy_obj) if source.stem == 'aGalaxy' else objects[i]
                                             for i, source in enumerate(original)) + '\n')
                source = ROOT / 'tests/test_e2e515_item_references.cpp'
                obj = staging / 'sanitized-item-test.o'
                run(['g++', *flags, *sanitizer_flags, '-c', str(source), '-o', str(obj)], 'sanitized-item-compile')
                binary = staging / 'sanitized-item-references'
                run(['g++', *sanitizer_flags, '@' + str(response), str(obj), '-Wl,--gc-sections',
                     str(library), '-Wl,-rpath,' + str(library.parent), '-lz', '-lpng', '-ljpeg',
                     '-lvorbisfile', '-lvorbis', '-logg', '-lSDL2', '-lm', '-pthread', '-o', str(binary)], 'sanitized-item-link')
                output = run(['env', 'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1',
                              'UBSAN_OPTIONS=halt_on_error=1', str(binary)], 'sanitized-item-run', cwd=staging)
                results.append(dict(name='sanitized-item-references', status='PASS', stdout=output,
                                    instrumented_units=['aGalaxy', 'item-reference-test'],
                                    remaining_game_units_instrumented=False, leak_detection=True,
                                    binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest()))
                print('sanitized-item-references: PASS', flush=True)
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
