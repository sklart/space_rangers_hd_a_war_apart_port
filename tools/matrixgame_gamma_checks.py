"""Addressed original gamma/LUT/state tests; no device execution or implicit NRO."""
import argparse
import hashlib
import json
import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/e2e558-gamma'
ORIGINAL = ROOT / 'build/matrixgame/e2e556-standard-nro/sdk/source'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def linux(path):
    return str(path).replace('\\', '/').replace('D:/', '/mnt/d/')


def function(source, marker):
    begin = source.index(marker)
    brace = source.index('{', begin)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]


def prepare(output):
    config = (ORIGINAL / 'MatrixGame/src/MatrixConfig.cpp').read_text(encoding='utf-8')
    upstream = (ROOT / 'upstream/matrixgame/MatrixGame/src/MatrixConfig.cpp').read_text(encoding='utf-8')
    gen = function(upstream, 'static void GenRamp(')
    assert function(config, 'static void GenRamp(') == gen.replace('WORD', 'std::uint16_t'), 'Original GenRamp algorithm changed'
    convert = function((ORIGINAL / 'portable-core/Math3D.hpp').read_text(encoding='utf-8'), 'inline int Float2Int(')
    apply = function(config, 'void CMatrixConfig::ApplyGammaRamp(')
    assert 'ReadyDevice().ApplyCalibratedGamma(0, newramp)' in apply
    values = function((ROOT / 'upstream/matrixgame/MatrixGame/src/MatrixConfig.hpp').read_text(encoding='utf-8'), 'struct SGammaVals') + ';'
    body = values + '\nstruct CMatrixConfig {SGammaVals m_GammaR,m_GammaG,m_GammaB;void ApplyGammaRamp();};\n' + convert + '\n' + gen + '\n' + apply + '\n'
    (output / 'original-gamma.inc').write_text(body, encoding='utf-8')
    return {'original_GenRamp_sha256': hashlib.sha256(gen.encode()).hexdigest(),
            'generated_ApplyGammaRamp_sha256': hashlib.sha256(apply.encode()).hexdigest(),
            'scope': 'Exact original methods; minimal declaration/routing fixture, no full Init'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('kind', choices=('host', 'arm64', 'windows', 'gl'))
    args = parser.parse_args()
    kind = args.kind
    out = BASE / kind
    out.mkdir(parents=True, exist_ok=True)
    report = {'status': 'INCOMPLETE', 'commands': [], 'Switch_hardware': 'NOT RUN / HARDWARE_PENDING'}
    names = ['tests/test_matrixgame_gamma.cpp', 'tests/test_matrixgame_gamma_gl.cpp',
             'tools/matrixgame_gamma_checks.py', 'port/matrixgame/native-engine/native_gamma.hpp',
             'port/matrixgame/native-engine/platform_3g.hpp', 'port/matrixgame/native-engine/platform_3g.cpp',
             'port/matrixgame/native-engine/native_presentation.hpp',
             'port/matrixgame/native-engine/native_diagnostic_pixels.hpp',
             'port/matrixgame/deko/gamma.vert.glsl', 'port/matrixgame/deko/gamma.frag.glsl']
    report['source_sha256_before'] = {name: sha(ROOT / name) for name in names}

    def run(command, label, env=None):
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace', env=env)
        log = result.stdout + result.stderr
        (out / (label + '.log')).write_text(log, encoding='utf-8')
        report['commands'].append({'label': label, 'command': command, 'exit': result.returncode, 'seconds': time.monotonic() - start})
        print(kind, label, result.returncode, log[-5000:] if result.returncode or label == 'runtime' else '', flush=True)
        if result.returncode:
            raise RuntimeError(label + ' failed')
        if label == 'runtime':
            report['stdout'] = log
        return log

    try:
        report['extraction'] = prepare(out)
        if kind == 'windows':
            vs = r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
            command = f'call "{vs}" -no_logo -arch=x64 -host_arch=x64 && cl /nologo /EHsc /std:c++20 /O2 /UNDEBUG /DNOMINMAX '
            command += f'/I"{out}" /I"{ROOT / "port/matrixgame/native-engine"}" /I"{ROOT / "port/matrixgame/native-core"}" '
            command += f'"{ROOT / "tests/test_matrixgame_gamma.cpp"}" "{ROOT / "port/matrixgame/native-engine/platform_3g.cpp"}" '
            command += f'/Fo"{out.as_posix()}/" /Fe"{out / "gamma.exe"}"'
            script = out / 'build.cmd'
            script.write_text(command + '\n')
            run(['cmd.exe', '/d', '/c', script], 'build')
            run([out / 'gamma.exe', out / 'ramps.bin'], 'runtime')
            report['binary_sha256'] = sha(out / 'gamma.exe')
        else:
            compiler = 'aarch64-linux-gnu-g++-15' if kind == 'arm64' else 'g++'
            flags = ['-std=c++20', '-O1', '-g', '-UNDEBUG', '-ffp-contract=off', '-fno-pie', '-no-pie']
            if kind == 'host':
                flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
            sources = [ROOT / 'tests/test_matrixgame_gamma.cpp', ROOT / 'port/matrixgame/native-engine/platform_3g.cpp']
            if kind == 'gl':
                sources = [ROOT / 'tests/test_matrixgame_gamma_gl.cpp']
            exe = out / 'gamma-test'
            run(['wsl', '--exec', compiler, *flags, '-I' + linux(out), '-I' + linux(ROOT / 'port/matrixgame/native-engine'), '-I' + linux(ROOT / 'port/matrixgame/native-core'), *map(linux, sources), '-o', linux(exe), *(['-lEGL', '-lGL'] if kind == 'gl' else [])], 'build')
            command = ['wsl', '--exec', 'env', 'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1', 'UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
            if kind == 'arm64':
                command += ['qemu-aarch64', '-L', '/usr/aarch64-linux-gnu']
            if kind == 'gl':
                command += ['LIBGL_ALWAYS_SOFTWARE=1', 'MESA_GL_VERSION_OVERRIDE=4.6', 'MESA_GLSL_VERSION_OVERRIDE=460', linux(exe), linux(ROOT / 'port/matrixgame/deko/gamma.vert.glsl'), linux(ROOT / 'port/matrixgame/deko/gamma.frag.glsl'), linux(BASE / 'host/ramps.bin')]
                report['input_ramps_sha256'] = sha(BASE / 'host/ramps.bin')
            else:
                command += [linux(exe), linux(out / 'ramps.bin')]
            run(command, 'runtime')
            report['binary_sha256'] = sha(exe)
        if kind != 'gl':
            report['ramps_sha256'] = sha(out / 'ramps.bin')
        report['status'] = 'PASS; ORIGINAL GAMMA/LUT/LIFETIME CPU ONLY' if kind != 'gl' else 'PASS; EXACT GLSL ON MESA; SWITCH NOT RUN'
    except Exception:
        report['status'] = 'FAIL'
        raise
    finally:
        if (out / 'original-gamma.inc').is_file():
            report['extracted_source_sha256'] = sha(out / 'original-gamma.inc')
        report['source_sha256_after'] = {name: sha(ROOT / name) for name in names}
        assert report['source_sha256_before'] == report['source_sha256_after'], 'Source changed during test'
        (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
