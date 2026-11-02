"""Exact original unmanaged D3DX request; controlled DDS inputs, no game writes."""
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/e2e561-unmanaged-bitmap'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixtures():
    out = BASE / 'fixtures'
    out.mkdir(parents=True, exist_ok=True)
    cases = []
    for width, height in ((1, 1), (1, 7), (3, 5), (6, 10), (7, 9), (8, 8), (13, 17), (32, 16), (8, 7), (3, 8), (2, 5), (4, 4), (2, 2)):
        for channels in (3, 4):
            for pattern in range(3):
                header = [0] * 32
                header[0:6] = [0x20534444, 124, 0x81007, height, width, width * height * channels]
                header[19:28] = [32, 0x41 if channels == 4 else 0x40, 0, channels * 8, 0xff0000, 0xff00, 0xff, 0xff000000 if channels == 4 else 0, 0x1000]
                payload = bytearray()
                for y in range(height):
                    for x in range(width):
                        if pattern == 0:
                            r, g, b, a = (x * 13 + y * 29 + 7) & 255, (x * 47 + y * 11 + 103) & 255, (x * 5 + y * 89 + 211) & 255, (x * 193 + y * 71) & 255
                        elif pattern == 1:
                            r, g, b, a = 135, 68, 229, 127
                        else:
                            r, g, b, a = (255 if (x + y) & 1 else 0), (255 if x & 1 else 0), (255 if y & 1 else 0), (255 if (x + y) & 1 else 0)
                        payload += bytes((b, g, r, a)[:channels])
                path = out / f'c{channels}-{width}x{height}-p{pattern}.dds'
                path.write_bytes(struct.pack('<32I', *header) + payload)
                for levels in (0, 1):
                    cases.append({'name': path.stem + '-L' + str(levels), 'dds': str(path.relative_to(ROOT)), 'sha256': sha(path),
                                  'channels': channels, 'width': width, 'height': height, 'pattern': pattern, 'requested_levels': levels})
    (BASE / 'cases.json').write_text(json.dumps(cases, indent=2) + '\n', encoding='utf-8')
    return cases


def main():
    global BASE
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=BASE)
    BASE = parser.parse_args().output.resolve()
    cases = fixtures()
    out = BASE / 'windows-reference'
    out.mkdir(parents=True, exist_ok=True)
    manifest = out / 'manifest.txt'
    manifest.write_text(''.join(str(c['requested_levels']) + '\t' + str(ROOT / c['dds']) + '\n' for c in cases), encoding='utf-8')
    cache = ROOT / 'build/matrixgame/e2e519/original-init-windows/CMakeCache.txt'
    values = {line.split(':', 1)[0]: line.split('=', 1)[1] for line in cache.read_text().splitlines() if ':' in line and '=' in line and not line.startswith(('//', '#'))}
    vs = r'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat'
    exe = out / 'unmanaged-reference.exe'
    command = f'call "{vs}" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /EHsc /std:c++20 /O2 /UNDEBUG /DNOMINMAX '
    command += f'/I"{values["MATRIXGAME_D3DX9_INCLUDE"]}" "{ROOT / "tests/probe_matrixgame_unmanaged_bitmap.cpp"}" /Fo"{out.as_posix()}/" /Fe"{exe}" /link d3d9.lib user32.lib "{values["MATRIXGAME_D3DX9_LIBRARY"]}"'
    script = out / 'build.cmd'
    script.write_text(command + '\n')
    report = {'status': 'INCOMPLETE', 'commands': [], 'scope': 'Actual Windows HAL DEFAULT texture allocation/descriptors; equivalent SYSTEMMEM request pixel export; no Draw/Present'}

    def run(cmd, label):
        cmd = list(map(str, cmd))
        result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
        log = result.stdout + result.stderr
        (out / (label + '.log')).write_text(log, encoding='utf-8')
        report['commands'].append({'label': label, 'command': cmd, 'exit': result.returncode})
        print(label, result.returncode, log[-7000:] if result.returncode or label == 'runtime' else '', flush=True)
        assert result.returncode == 0, label
        return log

    try:
        run(['cmd.exe', '/d', '/c', script], 'build')
        dll = Path(values['MATRIXGAME_D3DX9_LIBRARY']).parents[2] / 'bin/x86/D3DX9_43.dll'
        assert dll.is_file(), dll
        target = out / dll.name
        if not target.exists():
            shutil.copyfile(dll, target)
        assert sha(target) == sha(dll)
        report['D3DX_DLL_sha256'] = sha(target)
        report['stdout'] = run([exe, manifest, out], 'runtime')
        report['cases'] = []
        for case in cases:
            path = out / (case['name'] + '.oracle')
            data = path.read_bytes()
            magic, width, height, fmt, pool, usage, count = struct.unpack_from('<7I', data)
            assert magic == 0x31424d55 and pool == 0 and usage == 0
            offset = 28
            levels = []
            for i in range(count):
                w, h, size = struct.unpack_from('<3I', data, offset)
                offset += 12
                assert size == w * h * 4 and offset + size <= len(data)
                levels.append({'width': w, 'height': h, 'bytes': size, 'sha256': hashlib.sha256(data[offset:offset + size]).hexdigest()})
                offset += size
            assert offset == len(data)
            report['cases'].append({**case, 'output_width': width, 'output_height': height, 'format': fmt,
                                    'pool': pool, 'usage': usage, 'levels': levels, 'oracle': str(path.relative_to(ROOT)), 'oracle_sha256': sha(path)})
        report['status'] = 'PASS; EXACT WINDOWS REQUEST REFERENCE, NOT NATIVE RUNTIME'
        report['binary_sha256'] = sha(exe)
    finally:
        report['source_sha256'] = {name: sha(ROOT / name) for name in ('tests/probe_matrixgame_unmanaged_bitmap.cpp', 'tools/matrixgame_unmanaged_bitmap_reference.py')}
        (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
