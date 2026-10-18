"""Compare native texture preparation with captured real D3DX mip output."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import subprocess
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--runner-binary', type=Path)
    parser.add_argument('--qemu-sysroot', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    document = json.loads((Path(__file__).parent / 'fixtures/matrixgame/mip-control.json').read_text())
    paths = []
    expected = bytearray()
    boundaries = []
    def payload(case, field):
        if field in case:
            return base64.b64decode(case[field], validate=True)
        return zlib.decompress(base64.b64decode(case[field + '_zlib'], validate=True))
    for case in document['cases']:
        path = output / (case['name'] + '.image')
        path.write_bytes(payload(case, 'input'))
        paths.append(str(path))
        start = len(expected)
        expected += payload(case, 'expected')
        boundaries.append((case['name'], start, len(expected)))
    manifest = output / 'manifest.txt'
    manifest.write_text('\n'.join(paths) + '\n', encoding='utf-8')
    command = [str(args.binary.resolve())]
    if args.runner_binary:
        command += ['-L', str(args.qemu_sysroot.resolve()), str(args.runner_binary.resolve())]
    result = subprocess.run(command + [str(manifest)], capture_output=True, timeout=60)
    assert result.returncode == 0, (result.returncode, result.stderr.decode(errors='replace'))
    assert len(result.stdout) == len(expected), (len(result.stdout), len(expected))
    failures = []
    for name, start, end in boundaries:
        actual = result.stdout[start:end]
        wanted = expected[start:end]
        if actual != wanted:
            first = next(i for i, (a, b) in enumerate(zip(actual, wanted)) if a != b)
            failures.append((name, first, actual[first], wanted[first]))
    assert not failures, failures
    report = dict(scope='CPU mip preparation, exact captured D3DX bytes; no GPU or full resource-pack parity',
                  cases=len(paths), bytes=len(expected), sha256=hashlib.sha256(expected).hexdigest(),
                  reference_binary_sha256=document['reference_binary_sha256'],
                  reference_library_sha256=document['reference_library_sha256'],
                  reference_library_version=document['reference_library_version'])
    (output / 'oracle.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
