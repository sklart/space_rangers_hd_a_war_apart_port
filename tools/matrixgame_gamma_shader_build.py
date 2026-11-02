"""Compile/validate the private presentation LUT programs with the installed SDK."""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    header = '#pragma once\n#include <array>\n#include <cstdint>\nnamespace srhd_awa::matrixgame::gamma_compiled {\n'
    entries = {}
    compiler = args.sdk / 'tools/bin/uam'
    if not compiler.exists():
        compiler = compiler.with_suffix('.exe')
    for stage in ('vert', 'frag'):
        source = ROOT / f'port/matrixgame/deko/gamma.{stage}.glsl'
        output = args.output / f'gamma.{stage}.dksh'
        command = list(map(str, [compiler, '-s', stage, '-o', output, source]))
        subprocess.run(command, check=True)
        data = output.read_bytes()
        magic, head, control, code, program, count = struct.unpack_from('<6I', data)
        assert magic == 0x48534b44 and 24 <= head <= program < control
        assert code and control + code == len(data) and count == 1
        header += f'inline constexpr std::array<std::uint8_t,{len(data)}> {stage}' + '{' + ','.join(map(str, data)) + '};\n'
        entries[stage] = {'command': command, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                          'dksh_sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}
    (args.output / 'gamma_shaders.hpp').write_text(header + '}\n', encoding='utf-8')
    (args.output / 'gamma-shader-build.json').write_text(json.dumps(entries, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
