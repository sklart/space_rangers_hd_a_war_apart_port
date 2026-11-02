"""Full original ReadParams on real data.txt with a gamma-only CPU fixture."""
import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/e2e559-config-tail'
ENGINE = ROOT / 'build/matrixgame/e2e558-gamma/sdk/source'
DATA = ROOT / 'upstream/matrixgame/MatrixGame/CFG/robots/data.txt'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def linux(path):
    return str(path).replace('\\', '/').replace('D:/', '/mnt/d/')


def cache(path):
    return {line.split(':', 1)[0]: line.split('=', 1)[1] for line in path.read_text(encoding='utf-8').splitlines()
            if '=' in line and ':' in line and not line.startswith(('//', '#'))}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('kind', choices=('host', 'arm64', 'windows'))
    kind = parser.parse_args().kind
    out = BASE / kind
    out.mkdir(parents=True, exist_ok=True)
    names = ['tests/test_matrixgame_original_config.cpp', 'tools/matrixgame_config_tail_checks.py',
             'port/matrixgame/native-config-test/CMakeLists.txt',
             'port/matrixgame/native-engine/native_gamma.hpp', 'port/matrixgame/native-engine/platform_3g.cpp',
             'port/matrixgame/native-engine/platform_3g.hpp']
    report = {'status': 'INCOMPLETE', 'commands': [], 'source_sha256_before': {name: sha(ROOT / name) for name in names},
              'data_txt_sha256_before': sha(DATA), 'scope': 'Full actual original config TU and real parser/heaps; gamma-only routing fixture, NO GPU/Init/map'}

    def run(command, label):
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
        log = result.stdout + result.stderr
        (out / (label + '.log')).write_text(log, encoding='utf-8')
        report['commands'].append({'label': label, 'command': command, 'exit': result.returncode, 'seconds': time.monotonic() - start})
        print(kind, label, result.returncode, log[-6000:] if result.returncode or label == 'runtime' else '', flush=True)
        if result.returncode:
            raise RuntimeError(label + ' failed')
        return log

    try:
        manifest = json.loads((ENGINE / 'native-engine-source.json').read_text(encoding='utf-8'))['files_sha256']
        for name, digest in manifest.items():
            assert sha(ENGINE / name) == digest, name
        report['generated_input_sha256'] = manifest
        source = ROOT / 'port/matrixgame/native-config-test'
        build = out / ('build' if kind == 'windows' else 'build-native' if kind == 'host' else 'build-make')
        if kind == 'windows':
            cmake = Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe')
            prior = cache(ROOT / 'build/matrixgame/e2e519/original-init-windows/CMakeCache.txt')
            dependencies = [f'-D{key}={value}' for key, value in prior.items() if key in
                            ('ZLIB_INCLUDE_DIR', 'ZLIB_LIBRARY', 'ZLIB_LIBRARY_RELEASE', 'PNG_LIBRARY_RELEASE', 'PNG_PNG_INCLUDE_DIR', 'JPEG_LIBRARY_RELEASE', 'JPEG_INCLUDE_DIR')]
            run([cmake, '-S', source, '-B', build, '-G', 'Visual Studio 17 2022', '-A', 'Win32',
                 '-DBUILD_TESTING=OFF', '-DPython3_EXECUTABLE=' + sys.executable, '-DMATRIXGAME_ENGINE_SOURCE=' + str(ENGINE),
                 '-DMATRIXGAME_CONFIG_DEP_INCLUDES=' + ';'.join(str(ROOT / 'build/matrixgame/e2e519/windows' / name) for name in ('png', 'jpeg')), *dependencies], 'configure')
            run([cmake, '--build', build, '--config', 'Release', '--target', 'matrixgame-original-config-contract', '-j', '4'], 'build')
            exe = build / 'Release/matrixgame-original-config-contract.exe'
            report['stdout'] = run([exe, DATA, out / 'observed.bin'], 'runtime')
        else:
            options = ['-DBUILD_TESTING=OFF', '-DCMAKE_BUILD_TYPE=Debug', '-DCMAKE_CXX_FLAGS=-O1 -g -pthread', '-DCMAKE_EXE_LINKER_FLAGS=-no-pie', '-DCMAKE_MAKE_PROGRAM=/usr/bin/make']
            prior = cache(ROOT / f'build/matrixgame/e2e519/vector-group-fresh-{kind}/CMakeCache.txt')
            if kind == 'host':
                options += ['-DCORE_SANITIZERS=ON', '-DCMAKE_CXX_COMPILER=/usr/bin/g++']
            else:
                options += ['-DCMAKE_SYSTEM_NAME=Linux', '-DCMAKE_SYSTEM_PROCESSOR=aarch64', '-DCMAKE_CXX_COMPILER=/usr/bin/aarch64-linux-gnu-g++-15']
                options += ['-DMATRIXGAME_CONFIG_DEP_INCLUDES=' + ';'.join(linux(ROOT / 'build/matrixgame/e2e518/arm64-normalized' / name) for name in ('png', 'jpeg'))]
            for key in ('ZLIB_INCLUDE_DIR', 'ZLIB_LIBRARY', 'PNG_LIBRARY_RELEASE', 'PNG_PNG_INCLUDE_DIR', 'JPEG_LIBRARY_RELEASE', 'JPEG_INCLUDE_DIR'):
                value = prior.get(key) or prior.get(key + '_RELEASE')
                assert value and not value.endswith('-NOTFOUND'), key
                options.append(f'-D{key}={value}')
            run(['wsl', '--exec', 'cmake', '-S', linux(source), '-B', linux(build), '-G', 'Unix Makefiles',
                 '-DMATRIXGAME_ENGINE_SOURCE=' + linux(ENGINE), *options], 'configure')
            run(['wsl', '--exec', 'cmake', '--build', linux(build), '--target', 'matrixgame-original-config-contract', '-j', '4'], 'build')
            exe = build / 'matrixgame-original-config-contract'
            command = ['wsl', '--exec', 'env', 'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1', 'UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind == 'arm64':
                command += ['qemu-aarch64', '-L', '/usr/aarch64-linux-gnu']
            report['stdout'] = run([*command, linux(exe), linux(DATA), linux(out / 'observed.bin')], 'runtime')
        assert 'ORIGINAL FULL ReadParams returned=4' in report['stdout']
        report.update(status='PASS; FULL ORIGINAL ReadParams CPU ONLY', binary_sha256=sha(exe), observations_sha256=sha(out / 'observed.bin'))
    except Exception:
        report['status'] = 'FAIL'
        raise
    finally:
        report['source_sha256_after'] = {name: sha(ROOT / name) for name in names}
        report['data_txt_sha256_after'] = sha(DATA)
        assert report['source_sha256_after'] == report['source_sha256_before']
        assert report['data_txt_sha256_before'] == report['data_txt_sha256_after']
        (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
