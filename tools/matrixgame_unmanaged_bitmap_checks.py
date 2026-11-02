"""Focused original bitmap lifecycle and Windows pixel differential checks."""
import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/e2e561-unmanaged-bitmap'
ENGINE = ROOT / 'build/matrixgame/e2e558-gamma/sdk/source'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def linux(path):
    return str(path).replace('\\', '/').replace('D:/', '/mnt/d/')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('kind', choices=('host', 'arm64'))
    kind = parser.parse_args().kind
    out = BASE / kind
    out.mkdir(parents=True, exist_ok=True)
    report = {'status': 'INCOMPLETE', 'commands': [], 'scope': 'Actual original CBitmap/CTexture methods; CPU upload recorder; no GPU/Init/Switch'}

    def run(cmd, label):
        cmd = list(map(str, cmd))
        start = time.monotonic()
        p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
        log = p.stdout + p.stderr
        (out / (label + '.log')).write_text(log, encoding='utf-8')
        report['commands'].append({'label': label, 'command': cmd, 'exit': p.returncode, 'seconds': time.monotonic()-start})
        print(kind, label, p.returncode, log[-6000:], flush=True)
        assert p.returncode == 0, label
        return log

    try:
        prior = json.loads((ROOT / f'build/matrixgame/e2e522/unions-{kind}/result.json').read_text())
        cmd = [s for s in prior['commands'][0]['command'] if not s.endswith('.cpp')]
        old = linux(ROOT / 'build/matrixgame/e2e521/source')
        cmd = [s.replace(old, linux(ENGINE)) for s in cmd]
        core = ROOT / f'build/matrixgame/e2e559-config-tail/{kind}' / ('build-native' if kind=='host' else 'build-make') / 'core/libmatrixgame-native-core.a'
        cmd = [linux(core) if s.endswith('/core/libmatrixgame-native-core.a') else s for s in cmd]
        sources = ['tests/test_matrixgame_unmanaged_bitmap.cpp'] + ['port/matrixgame/native-engine/'+s for s in
            ('native_texture.cpp', 'platform_3g.cpp', 'legacy_texture_backend.cpp', 'unmanaged_texture_backend.cpp', 'native_surfaces.cpp', 'offscreen_backend.cpp')]
        generated = [ENGINE / 'MatrixLib/3G' / s for s in ('OriginalManagedTexture.cpp', 'OriginalUnmanagedTexture.cpp')]
        first = next(i for i,s in enumerate(cmd) if s.endswith('.a'))
        cmd[first:first] = [linux(ROOT/s) for s in sources] + list(map(linux, generated))
        exe = out / 'bitmap-contract'
        cmd[cmd.index('-o')+1] = linux(exe)
        report['source_sha256'] = {str(p.relative_to(ROOT)):sha(p) for p in [*(ROOT/s for s in sources), *generated,
            ROOT/'port/matrixgame/native-engine/unmanaged_bitmap_filter.hpp', ROOT/'tools/matrixgame_unmanaged_bitmap_checks.py']}
        report['archive_sha256'] = {s:sha(Path(s.replace('/mnt/d/', 'D:/'))) for s in cmd if s.endswith('.a')}
        reference = BASE/'expanded'
        cases = json.loads((reference/'cases.json').read_text())
        manifest = out/'manifest.txt'
        manifest.write_text(''.join(f"{c['requested_levels']} {c['pattern']} {linux(ROOT/c['dds'])} {linux(reference/'windows-reference'/(c['name']+'.oracle'))}\n" for c in cases))
        report['reference_sha256'] = sha(reference/'windows-reference/result.json')
        run(cmd, 'build')
        cmd = ['wsl', '--exec', 'env', 'ASAN_OPTIONS=detect_leaks=1:halt_on_error=1', 'UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
        if kind=='arm64':cmd += ['qemu-aarch64', '-L', '/usr/aarch64-linux-gnu']
        report['stdout'] = run(cmd+[linux(exe),linux(manifest),linux(out/'observed.bin')], 'runtime')
        report.update(status='PASS; ORIGINAL BITMAP CPU DIFFERENTIAL ONLY', binary_sha256=sha(exe), observations_sha256=sha(out/'observed.bin'))
    finally:
        (out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__ == '__main__':
    main()
