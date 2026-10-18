"""Independent STRG-to-BlockPar tree oracle; original/native parser runner."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
from probe_matrixgame_native_storage import parse_storage, zl03


def trees(data):
    # Validate all bounds/types using the independent storage oracle first.
    parse_storage(data)
    body = zl03(data[8:]) if struct.unpack_from('<I', data, 4)[0] else data[8:]
    position = 0
    def u32():
        nonlocal position
        value = struct.unpack_from('<I', body, position)[0]; position += 4
        return value
    def name():
        nonlocal position
        start = position
        while body[position:position+2] != b'\0\0': position += 2
        result = body[start:position].decode('utf-16-le', 'surrogatepass'); position += 2
        return result
    tables = {}
    for _ in range(u32()):
        table = name(); columns = {}
        for _ in range(u32()):
            key = name(); kind = u32(); size = u32()
            blob = body[position:position+size]; position += size
            if kind & 0x80000000: blob = zl03(blob)
            if kind & 0x7fffffff != 5: raise ValueError('configuration column is not WCHAR')
            table_at, count, element = struct.unpack_from('<III', blob)
            arrays = []
            for index in range(count):
                offset, length, _capacity = struct.unpack_from('<III', blob, table_at + index * 12)
                arrays.append(blob[offset:offset+length*element])
            if key in columns: raise ValueError('duplicate column')
            columns[key] = arrays
        if table in tables: raise ValueError('duplicate table')
        tables[table] = columns
    return tables


def tree_digest(tables, root):
    value = 14695981039346656037
    blocks = parameters = 0
    def feed(data):
        nonlocal value
        for byte in data: value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    def text(data): feed(struct.pack('<I', len(data))); feed(data)
    def visit(key, ancestors):
        nonlocal blocks, parameters
        if key in ancestors or len(ancestors) >= 128: raise ValueError('configuration cycle/depth')
        row = tables[key]
        if set(row) != {'0','1','2','3'}: raise ValueError('configuration schema')
        names, values, children, ids = (row[str(i)] for i in range(4))
        if len(names) != len(values) or len(children) != len(ids): raise ValueError('paired column count')
        blocks += 1; parameters += len(names)
        feed(struct.pack('<II', len(names), len(children)))
        for name, content in zip(names, values): text(name); text(content)
        for name, child in zip(children, ids):
            text(name); visit(child.decode('utf-16-le', 'surrogatepass'), ancestors | {key})
    visit(root, set())
    return {'tree_fnv64': f'{value:016x}', 'blocks': blocks, 'parameters': parameters}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--config', type=Path)
    parser.add_argument('--reference', type=Path)
    parser.add_argument('--runner-binary', type=Path)
    parser.add_argument('--qemu-sysroot', type=Path)
    args = parser.parse_args()
    command = [str(args.binary.resolve())]
    if args.runner_binary:
        if not args.qemu_sysroot: parser.error('--runner-binary requires --qemu-sysroot')
        command += ['-L', str(args.qemu_sysroot.resolve()), str(args.runner_binary.resolve())]
    output = args.output.resolve(); command.append(str(output))
    if args.config: command.append(str(args.config.resolve()))
    if os.name == 'nt': ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    print(result.stdout, end=''); print(result.stderr, end='', file=sys.stderr); result.check_returncode()
    rows = [json.loads(line) for line in result.stdout.splitlines() if line.startswith('{')]
    # Recorded from the pinned unmodified MSVC x86 parser, including all 65536 units.
    assert rows[0] == {'numeric_fnv64': 'fbe5b145ba008067',
        'tree_fnv64': '469a674f3ada5d45', 'blocks': 4, 'parameters': 5}
    unicode_text = (output/'fixture-utf16.txt').read_bytes().decode('utf-16')
    narrow_text = (output/'fixture-utf8.txt').read_bytes().decode('utf-8')
    assert unicode_text == narrow_text
    serialized = (output/'fixture-config.strg').read_bytes()
    controlled = tree_digest(trees(serialized), 'da')
    assert all(rows[0][key] == value for key, value in controlled.items())
    proof = {'scope': 'CPU configuration parser/storage bridge only', 'controlled': rows[0],
        'fixture_sha256': hashlib.sha256(serialized).hexdigest(),
        'text_utf16_sha256': hashlib.sha256((output/'fixture-utf16.txt').read_bytes()).hexdigest()}
    if args.config:
        config = args.config.read_bytes(); graph = trees(config)
        proof['real_config_sha256'] = hashlib.sha256(config).hexdigest()
        proof['roots'] = {}
        for root, row in zip(('da','if'), rows[1:]):
            expected = tree_digest(graph, root)
            assert row['real_root'] == root and all(row[key] == value for key, value in expected.items())
            proof['roots'][root] = expected
        assert set(proof['roots']) == {'da','if'}
    if args.reference:
        reference = json.loads(args.reference.read_text())
        assert proof == reference
    (output/'oracle.json').write_text(json.dumps(proof, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'independent_blockpar_oracle': 'PASS', **proof}))


if __name__ == '__main__':
    main()
