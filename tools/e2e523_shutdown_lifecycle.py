"""Build a separate host shutdown candidate from an existing effective build.

Only the two proven worker finalizers change; frozen generated files/objects
are read-only inputs. The lifecycle target instruments the changed script TU,
EC_Thread and the event/thread/handle adapters, not the entire game.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from e2e_source_overrides import generated_source

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, log, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, **kwargs)
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'{command[0]} failed; see {log}')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, default=ROOT / 'port/switch/build/e2e5-host-game/switch-effective')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/e2e523/exit-candidate')
    args = parser.parse_args()
    base, out = args.base.resolve(), args.output.resolve()
    if out == base or base in out.parents:
        parser.error('candidate output must be separate from the base build')
    out.mkdir(parents=True, exist_ok=True)
    baseline = json.loads((base / 'report.json').read_text())
    flags = json.loads((base / 'compile-flags.json').read_text())['flags']
    sources = {}
    for name in ('aScript.cpp', 'Rangers.cpp'):
        original = ROOT / 'upstream/cpp/src' / name
        fresh = generated_source(original, out / 'fresh' / name,
                                 audio_on=True, e2e5_profile=True, script_width=True)
        fresh_text = fresh.read_text()
        frozen = ROOT / baseline['source_map'][original.relative_to(ROOT).as_posix()]
        text = frozen.read_text()
        if name == 'aScript.cpp':
            start = '    void FinalizeScriptEngine() {'
            end = '    void RunGlobalScriptsForContext('
            replacement = fresh_text[fresh_text.index(start):fresh_text.index(end)]
            text = text[:text.index(start)] + replacement + text[text.index(end):]
        else:
            at = fresh_text.index('                                        if (SteamCallbackThread != nullptr) {')
            start = fresh_text.rfind('#if !defined(__SWITCH__)', 0, at)
            end = fresh_text.index('#endif', at) + len('#endif')
            replacement = fresh_text[start:end]
            at = text.index('                                        SteamCallbackThread->RequestStop();')
            start = text.rfind('#if !defined(__SWITCH__)', 0, at)
            end = text.index('#endif', at) + len('#endif')
            text = text[:start] + replacement + text[end:]
        source = out / name
        source.write_text(text)
        sources[name] = source
    library = base / 'okgf/libokgf.so'
    if not (out / 'okgf').exists():
        (out / 'okgf').symlink_to(base / 'okgf', target_is_directory=True)
    objects = [Path(line.strip().strip('"')) for line in (base / 'link-objects.rsp').read_text().splitlines() if line.strip()]
    replacements = {}
    for name, source in sources.items():
        obj = out / (source.stem + '.o')
        run(['g++', *flags, '-c', str(source), '-o', str(obj)], out / (source.stem + '-compile.log'))
        replacements[source.stem + '.o'] = obj
    link_flags = ['-Wl,--gc-sections', str(library), '-Wl,-rpath,' + str(library.parent),
                  '-lz', '-lpng', '-ljpeg', '-lvorbisfile', '-lvorbis', '-logg', '-lSDL2', '-lm', '-pthread']
    with tempfile.TemporaryDirectory(prefix='e2e523-shutdown-') as folder:
        staging = Path(folder)
        staged = []
        for index, obj in enumerate(objects):
            target = staging / f'{index:03d}.o'
            shutil.copyfile(replacements.get(obj.name, obj), target)
            staged.append(target)
        response = staging / 'objects.rsp'
        response.write_text('\n'.join(map(str, staged)) + '\n')
        run(['g++', '@' + str(response), *link_flags, '-o', str(out / 'original-game-host')], out / 'link.log')
        sanitizers = ['-O1', '-fsanitize=address,undefined', '-fno-sanitize=vptr', '-fno-omit-frame-pointer']
        instrumented = [sources['aScript.cpp'],
                        ROOT / baseline['source_map']['upstream/cpp/src/EC_Thread.cpp'],
                        *(ROOT / 'port/switch/platform' / name for name in
                          ('e2e_events.cpp', 'e2e_threads.cpp', 'win32_compat_sync.cpp', 'win32_handles.cpp')),
                        ROOT / 'tests/test_e2e523_script_shutdown.cpp']
        for index, source in enumerate(instrumented):
            obj = staging / (source.stem + '-asan.o')
            run(['g++', *flags, *sanitizers, '-c', str(source), '-o', str(obj)], out / (source.stem + '-asan-compile.log'))
            replace_name = 'e2e5_host_game_main.o' if index == len(instrumented) - 1 else source.stem + '.o'
            staged[ next(i for i, old in enumerate(objects) if old.name == replace_name) ] = obj
        response.write_text('\n'.join(map(str, staged)) + '\n')
        test = out / 'script-shutdown-test'
        run(['g++', '@' + str(response), *link_flags, *sanitizers, '-o', str(test)], out / 'test-link.log')
    result = run([str(test)], out / 'test-runtime.log', cwd=out, timeout=15)
    actual_map = dict(baseline['source_map'])
    actual_hashes = dict(baseline['compiled_sources_sha256'])
    actual_units = [dict(unit) for unit in baseline['compiled_units']]
    for name, source in sources.items():
        original = 'upstream/cpp/src/' + name
        old_source = actual_map[original]
        actual_hashes.pop(old_source, None)
        actual_map[original] = source.relative_to(ROOT).as_posix()
        actual_hashes[actual_map[original]] = digest(source)
        for unit in actual_units:
            if unit['original'] == original:
                unit.update(effective=actual_map[original],
                            object=replacements[Path(name).stem + '.o'].relative_to(ROOT).as_posix(),
                            object_sha256=digest(replacements[Path(name).stem + '.o']))
    report = dict(baseline, binary=str(out / 'original-game-host'),
                  binary_sha256=digest(out / 'original-game-host'),
                  baseline_binary_sha256=digest(base / 'original-game-host'),
                  baseline_git_head=baseline.get('git_head'), git_head=None, git_dirty=True,
                  source_map=actual_map, compiled_this_run=2, linked_this_run=True,
                  compiled_sources_sha256=actual_hashes, compiled_units=actual_units,
                  instrumented_sources={str(source.relative_to(ROOT)): digest(source) for source in instrumented},
                  candidate_scope='only ScriptRequestThread and host Steam finalizers; no rescale/save/render changes',
                  candidate_sources={name: {'path': str(source), 'sha256': digest(source)} for name, source in sources.items()},
                  lifecycle_test='HOST PASS; selected TUs ASan/UBSan; original thread/event methods; full game not instrumented',
                  lifecycle_stdout=result.stdout, hardware_executed=False)
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in ('binary_sha256', 'candidate_scope', 'lifecycle_test', 'lifecycle_stdout')}, indent=2))


if __name__ == '__main__':
    main()
