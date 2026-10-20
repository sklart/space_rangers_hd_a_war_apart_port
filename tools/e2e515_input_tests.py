"""Focused host suite for semantic routing plus the existing input contracts."""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
TESTS = ('e2e515_controller_actions', 'e2e515_text_input', 'e2e515_profiles', 'm27_input_platform', 'e2e54_touch',
         'e2e55_input', 'e2e56_input', 'e2e5_input_history',
         'e2e59_map_swipe', 'e2e511_arcade_commands')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    output = ROOT / 'build/e2e515' / ('input-asan' if args.sanitize else 'input-host')
    output.mkdir(parents=True, exist_ok=True)
    report = output / 'result.json'
    report.write_text(json.dumps({'status': 'RUNNING'}))
    results = []
    for name in TESTS:
        binary = output / name
        command = [os.environ.get('CXX', 'g++'), '-std=c++20', '-O1', '-g',
                   '-Wall', '-Wextra', '-Iport/switch/platform']
        if args.sanitize:
            command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        command += [f'tests/test_{name}.cpp', 'port/switch/platform/input_platform.cpp', '-o', str(binary)]
        for phase, invocation in [('build', command), ('run', [str(binary)])]:
            result = subprocess.run(invocation, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=120)
            (output / f'{name}-{phase}.log').write_text(result.stdout)
            if result.returncode:
                report.write_text(json.dumps({'status': 'FAIL', 'test': name, 'phase': phase,
                                              'exit': result.returncode}, indent=2))
                print(result.stdout, flush=True)
                raise SystemExit(result.returncode)
        results.append({'test': name, 'status': 'PASS'})
        print(f'{name}: PASS', flush=True)
    report.write_text(json.dumps({'status': 'PASS', 'sanitizers': args.sanitize,
                                 'tests': results}, indent=2) + '\n')


if __name__ == '__main__':
    main()
