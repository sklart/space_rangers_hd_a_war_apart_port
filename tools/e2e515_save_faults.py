"""Fault and process-stop checks on read-only copies of complete real RSG64 files."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'build/e2e515/save-faults'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--old', type=Path, required=True)
    parser.add_argument('--new', type=Path, required=True)
    args = parser.parse_args()
    sources = [args.old.resolve(strict=True), args.new.resolve(strict=True)]
    for source in sources:
        if (ROOT / 'build') not in source.parents or not source.is_file() or source.is_symlink():
            parser.error('Inputs must be isolated build fixtures')
    before = [hashlib.sha256(p.read_bytes()).hexdigest() for p in sources]
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / 'result.json').write_text(json.dumps({'status': 'RUNNING'}))
    binary = OUTPUT / 'manual-recovery'
    command = ['g++', '-std=c++20', '-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
               '-Iport/switch/platform', 'tests/test_e2e515_manual_recovery.cpp',
               'port/switch/platform/e2e4_quicksave_transaction.cpp', 'port/switch/platform/user_root.cpp',
               '-lz', '-o', str(binary)]
    built = subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (OUTPUT / 'build.log').write_text(built.stdout)
    if built.returncode:
        print(built.stdout); raise RuntimeError(f'Manual recovery build: {built.returncode}')
    legacy = OUTPUT / 'legacy-transactions'
    legacy_command = command.copy()
    legacy_command[legacy_command.index('tests/test_e2e515_manual_recovery.cpp')] = 'tests/test_e2e4_quicksave_transaction.cpp'
    legacy_command[-1] = str(legacy)
    for phase, invocation in (('build', legacy_command), ('run', [str(legacy), str(OUTPUT / 'legacy-runs')])):
        checked = subprocess.run(invocation, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, timeout=300)
        (OUTPUT / ('legacy-' + phase + '.log')).write_text(checked.stdout)
        if checked.returncode:
            print(checked.stdout); raise RuntimeError(f'Legacy transaction {phase}: {checked.returncode}')
    stage = OUTPUT / 'runs' / uuid.uuid4().hex
    stage.parent.mkdir(exist_ok=True)
    run = subprocess.run([str(binary), *map(str, sources), str(stage)], cwd=ROOT,
                         text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
    (OUTPUT / 'run.log').write_text(run.stdout)
    unchanged = [hashlib.sha256(p.read_bytes()).hexdigest() for p in sources] == before
    report = dict(status='PASS' if run.returncode == 0 and unchanged else 'FAIL',
                  returncode=run.returncode, output=run.stdout, stage=str(stage),
                  source_sha256=before, sources_unchanged=unchanged, sanitizers=True, legacy_transactions='PASS',
                  binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(), hardware_executed=False)
    (OUTPUT / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2), flush=True)
    if report['status'] != 'PASS': raise SystemExit(1)

if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        OUTPUT.mkdir(parents=True, exist_ok=True)
        (OUTPUT / 'result.json').write_text(json.dumps({'status': 'FAIL', 'error': str(error)}, indent=2) + '\n')
        raise
