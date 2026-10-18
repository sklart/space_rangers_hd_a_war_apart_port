"""Independent register transcript; optional real D3D9 Set/Get, never GPU proof."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
ROOT = Path(__file__).resolve().parents[1]
SOURCES = {
    'MatrixLib/3G/3g.cpp': 'e02ae38d13e88a6b83d893f974cbfeebf936938e01c7f03097cd8be32a29605f',
    'MatrixLib/3G/VectorObject.cpp': '11ffb4eb842b5cdf6041d0ddc3bf25a124b06bdd8ac36f090b4c2ac6d9586303',
    'MatrixGame/src/Interface/CIFaceElement.cpp': '288669c8ad3e3c709cb3c70bcff453b1607fbfdf63d7dfad42d7dd6caf70c251',
    'MatrixGame/src/MatrixSampleStateManager.cpp': '6d2562c1ba84c166023efbdff1b16b92a7e0904bd5a22474d17037f8702b9b1e',
}
REGS = {'ADDRESSU': 1, 'ADDRESSV': 2, 'ADDRESSW': 3, 'MAGFILTER': 5,
        'MINFILTER': 6, 'MIPFILTER': 7, 'MIPMAPLODBIAS': 8}
CONSTANTS = {'D3DTADDRESS_WRAP': 1, 'D3DTADDRESS_CLAMP': 3,
             'D3DTEXF_NONE': 0, 'D3DTEXF_POINT': 1, 'D3DTEXF_LINEAR': 2}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--reference-binary', type=Path)
    p.add_argument('--runner-binary', type=Path)
    p.add_argument('--qemu-sysroot', type=Path)
    a = p.parse_args()
    out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    operations = []; inventory = {}
    for name, wanted in SOURCES.items():
        data = (ROOT/'upstream/matrixgame'/name).read_bytes().replace(b'\r\n', b'\n')
        assert hashlib.sha256(data).hexdigest() == wanted, name
        text = re.sub(r'/\*.*?\*/|//[^\n]*', '', data.decode(), flags=re.S)
        count = 0
        for reg, value in re.findall(r'(?:SetSamplerState|SetState)\(\s*[^,]+,\s*D3DSAMP_(\w+)\s*,\s*(\w+)\s*\)', text):
            if reg in REGS and value in CONSTANTS:
                operations.append((REGS[reg], CONSTANTS[value])); count += 1
        inventory[name] = count
    # The extracted constants are fixtures, not execution of original branches.
    # Explicit mixed filter controls retain linear mip through point min/mag.
    bits = lambda f: struct.unpack('<I', struct.pack('<f', f))[0]
    operations += [(7, 2), (8, bits(-1.0)), (5, 1), (6, 1), (1, 3), (2, 1)]
    choices = {1: (1, 3), 2: (1, 3), 3: (1, 3), 5: (1, 2), 6: (1, 2), 7: (0, 1, 2),
               8: tuple(bits(f) for f in (-15., -3.25, -1., -0., 0., .5, 1., 3.75, 15.))}
    for reg, values in choices.items():
        operations.extend((reg, value) for value in values)
    seed = 0x71643891; keys = list(choices)
    for _ in range(2000):
        seed = (seed*1664525+1013904223) & 0xffffffff; reg = keys[seed % len(keys)]
        seed = (seed*1664525+1013904223) & 0xffffffff
        operations.append((reg, choices[reg][seed % len(choices[reg])]))
    order = list(REGS.values()); current = [1, 1, 1, 1, 1, 0, 0]
    expected = bytearray(struct.pack('<7I', *current))
    for reg, value in operations:
        current[order.index(reg)] = value
        expected += struct.pack('<7I', *current)
    manifest = out/'registers.txt'
    manifest.write_text(''.join(f'{r} {v}\n' for r, v in operations))
    command = [str(a.binary.resolve())]
    if a.runner_binary:
        command += ['-L', str(a.qemu_sysroot.resolve()), str(a.runner_binary.resolve())]
    def execute(command, label):
        r = subprocess.run(command+[str(manifest)], capture_output=True, timeout=90)
        (out/(label+'-stderr.log')).write_bytes(r.stderr)
        assert r.returncode == 0, (label, r.returncode, r.stderr[-2000:])
        assert r.stdout == expected, (label, len(r.stdout), len(expected),
            next((i for i, (x, y) in enumerate(zip(r.stdout, expected)) if x != y), None))
    execute(command, 'native')
    if a.reference_binary:
        execute([str(a.reference_binary.resolve())], 'd3d9')
    result = dict(scope='Sampler registers only; no draw, GPU image or battle',
        operations=len(operations), source_constants=inventory, transcript_bytes=len(expected),
        sha256=hashlib.sha256(expected).hexdigest(), d3d9_nullref_match=bool(a.reference_binary),
        unsupported='anisotropic manager path, mirror/border, sRGB, max mip, displacement')
    (out/'oracle.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
