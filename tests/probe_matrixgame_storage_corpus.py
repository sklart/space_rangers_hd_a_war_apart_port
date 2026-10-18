"""Run real VO storage load/export checks without distributing game resources."""
import argparse
import json
import mmap
from pathlib import Path
import subprocess
import sys
from probe_ui_images_release import entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--representative', action='store_true', help='One VO per directory plus three largest; default checks every VO')
    parser.add_argument('--runner-binary', type=Path)
    parser.add_argument('--qemu-sysroot', type=Path)
    args = parser.parse_args()
    with args.package.open('rb') as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as data:
        rows = [row for row in entries(data) if row[0].lower().endswith('.vo')]
    if not rows:
        raise ValueError('No VO entries in package')
    if args.representative:
        selected = {}
        for row in rows:
            selected.setdefault(row[0].rsplit('/', 1)[0], row)
        rows = list({row[0]: row for row in list(selected.values()) + sorted(rows, key=lambda row: row[2], reverse=True)[:3]}.values())
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for index, row in enumerate(rows):
        case = output / str(index)
        command = [sys.executable, str(Path(__file__).with_name('probe_matrixgame_native_storage.py')),
            '--binary', str(args.binary.resolve()), '--output', str(case),
            '--package', str(args.package.resolve()), '--entry', row[0]]
        if args.runner_binary:
            if not args.qemu_sysroot:
                parser.error('--runner-binary requires --qemu-sysroot')
            command += ['--runner-binary', str(args.runner_binary.resolve()), '--qemu-sysroot', str(args.qemu_sysroot.resolve())]
        result = subprocess.run(command, capture_output=True, text=True, timeout=150)
        (output / f'{index}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode:
            raise RuntimeError(f'{row[0]}: exit {result.returncode}; see {output / f"{index}.log"}')
        evidence = json.loads((case / 'oracle.json').read_text())['real']
        results.append(evidence)
        print(json.dumps({'index': index, **evidence}), flush=True)
    report = {'scope': 'STRG CPU resource contract; no rendering/hardware proof',
        'representative': args.representative, 'count': len(results),
        'source_bytes': sum(row['source_bytes'] for row in results), 'cases': results}
    (output / 'summary.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'storage_corpus': 'PASS', 'count': report['count'], 'source_bytes': report['source_bytes']}))


if __name__ == '__main__':
    main()
