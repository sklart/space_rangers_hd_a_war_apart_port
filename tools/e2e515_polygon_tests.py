"""Test the bounded memo against the actual effective game projector and RTL."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'port/switch/build/e2e5-host-game/optimized'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    targets = parser.add_mutually_exclusive_group()
    targets.add_argument('--arm64', action='store_true')
    targets.add_argument('--sdk', action='store_true')
    parser.add_argument('--inline', dest='inline_projection', action='store_true')
    args = parser.parse_args()
    stage = ROOT / 'build/e2e515/polygon-contract' / ('sdk' if args.sdk else 'arm64' if args.arm64 else 'asan')
    if args.inline_projection: stage = stage.with_name('inline-' + stage.name)
    stage.mkdir(parents=True, exist_ok=True)
    result = stage / 'result.json'; result.write_text(json.dumps({'status': 'RUNNING'}))
    if args.sdk:
        from e2e515_polygon_overrides import apply
        sdk = ROOT / 'port/switch/build/e2e-game-save-test'
        mapping = json.loads((sdk / 'effective-source-map.json').read_text())
        source = ROOT / mapping['upstream/cpp/src/ab_Polygon.cpp']
        generated = stage / 'polygon.cpp'; generated.write_text(apply(source.read_text()))
        if args.inline_projection:
            from e2e515_polygon_overrides import apply_inline
            generated.write_text(apply_inline(source.read_text(), (ROOT / mapping['upstream/cpp/src/ab_Global.cpp']).read_text()))
        config = json.loads((sdk / 'compile-flags.txt').read_text())
        obj = stage / 'polygon.o'
        run = subprocess.run([config['compiler'], *config['flags'], '-I' + str(ROOT / 'tests'), '-c', str(generated), '-o', str(obj)],
                             cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
        (stage / 'build.log').write_text(run.stdout)
        report = dict(status='COMPILE PASS' if not run.returncode else 'FAIL', linked=False,
                      inline_projection=args.inline_projection,
                      hardware_executed=False, returncode=run.returncode, compiler=config['compiler'],
                      source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                      generated_sha256=hashlib.sha256(generated.read_bytes()).hexdigest())
        if not run.returncode: report['object_sha256'] = hashlib.sha256(obj.read_bytes()).hexdigest()
        result.write_text(json.dumps(report, indent=2) + '\n'); print(json.dumps(report, indent=2))
        if run.returncode: print(run.stdout); raise SystemExit(1)
        return
    mapping = json.loads((BUILD / 'report.json').read_text())['source_map']
    source = ROOT / mapping['upstream/cpp/src/ab_Global.cpp']
    text = source.read_text(); start = text.index('    EC_Struct::TVector3D ProjectPointByMatrix(')
    end = text.index('\n    }', start) + len('\n    }')
    generated = stage / 'actual-projector.cpp'
    generated.write_text('#include "runtime_support.hpp"\n#include "units/ab_Global.hpp"\nnamespace ab_Global {\n' + text[start:end] + '\n}\n')
    flags = ['-std=gnu++20', '-O2', '-fno-strict-aliasing', '-ffunction-sections', '-fdata-sections', '-Wno-invalid-offsetof']
    if args.inline_projection:
        from e2e515_polygon_overrides import inline_projector
        (stage / 'inline-projector.hpp').write_text(inline_projector(text))
        flags += ['-DE2E515_PROJECTION_INLINE', '-I' + str(stage)]
    for path in (ROOT / 'tests', ROOT / 'port/switch/platform', BUILD / 'source-overlay', ROOT / 'upstream/cpp/src',
                 BUILD / 'runtime-overlay', ROOT / 'upstream/cpp/runtime', ROOT / 'upstream/okgf/include'):
        flags += ['-I' + str(path)]
    if not args.arm64: flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    binary = stage / 'test'
    command = ['aarch64-linux-gnu-g++' if args.arm64 else 'g++', *flags, str(generated),
               str(ROOT / 'tests/test_e2e515_polygon_projection.cpp'), str(ROOT / 'upstream/cpp/runtime/runtime.cpp'),
               '-Wl,--gc-sections', '-lm', '-pthread', '-o', str(binary)]
    for phase, invocation in (('build', command), ('run', ['qemu-aarch64', '-L', '/usr/aarch64-linux-gnu', str(binary)] if args.arm64 else [str(binary)])):
        run = subprocess.run(invocation, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=600)
        (stage / (phase + '.log')).write_text(run.stdout)
        if run.returncode:
            result.write_text(json.dumps({'status': 'FAIL', 'phase': phase, 'returncode': run.returncode}))
            print(run.stdout[-6000:]); raise SystemExit(1)
    report = dict(status='PASS', arm64_qemu=args.arm64, sanitizers=not args.arm64, output=run.stdout,
                  inline_projection=args.inline_projection,
                  source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                  generated_sha256=hashlib.sha256(generated.read_bytes()).hexdigest(),
                  binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(), hardware_executed=False)
    if args.inline_projection:
        report['inline_source_sha256'] = hashlib.sha256((stage / 'inline-projector.hpp').read_bytes()).hexdigest()
    result.write_text(json.dumps(report, indent=2) + '\n'); print(json.dumps(report, indent=2), flush=True)

if __name__ == '__main__': main()
