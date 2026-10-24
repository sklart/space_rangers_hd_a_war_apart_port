"""Audit the actual Init candidate without rebuilding or modifying protected inputs."""
import hashlib
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from matrixgame_vector_object_overrides import function


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    base = ROOT / 'build/matrixgame/e2e519'
    source = base / 'original-init-source'
    old = base / 'output-paths-source'
    name = 'MatrixGame/src/MatrixGame.cpp'
    before = (old / name).read_text(encoding='utf-8')
    after = (source / name).read_text(encoding='utf-8')
    restored = after.replace('\n    original_game_owner::ValidateEntry();', '', 1)
    restored = restored.replace('\n    original_game_owner::ObserveStatics();', '', 1)
    restored = restored.replace('\n    original_game_owner::ValidateDeinit();', '', 1)
    restored = restored.removeprefix('#include "original_game.hpp"\nnamespace original_game_owner=srhd_awa::matrixgame::original_game;\n')
    assert restored == before, 'Unexpected changes beyond the three ownership hooks'
    assert function(after, 'static void static_init(void)') == function(before, 'static void static_init(void)')
    header_diffs = [str(p.relative_to(old)) for p in old.rglob('*.hpp')
                    if p.read_bytes() != (source / p.relative_to(old)).read_bytes()]
    assert not header_diffs, header_diffs
    git = r'C:\Program Files\Git\cmd\git.exe'
    parent = subprocess.check_output([git, 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    header = 'port/matrixgame/native-engine/init_runtime.hpp'
    previous = subprocess.check_output([git, 'show', 'HEAD:' + header], cwd=ROOT).decode('utf-8')
    current = (ROOT / header).read_text(encoding='utf-8')
    marker = '    std::unique_ptr<Base::CHeap> matrix_heap_;'
    assert previous[previous.index(marker):] == current[current.index(marker):]
    results = {}
    for kind in ('host', 'arm64'):
        result = json.loads((base / ('original-init-' + kind) / 'result.json').read_text(encoding='utf-8'))
        assert result['CGame_Init_actual_call'] and result['original_inputs_unchanged']
        assert all(v['exit'] == 0 for v in result['results'].values())
        assert 'legacy DLL entry requires registered borrowed Window/Device providers' in result['results']['real']['stdout']
        results[kind] = result
    sdk = json.loads((base / 'original-init-sdk/link-result.json').read_text(encoding='utf-8'))
    assert sdk['link_exit'] == 0 and not sdk['strong_undefined']
    msvc = json.loads((base / 'original-init-msvc/result.json').read_text(encoding='utf-8'))
    assert msvc['compile_exit'] == 0
    protected = json.loads((ROOT / 'reports/matrixgame-e2e519-output-paths-evidence.json').read_text(encoding='utf-8'))['protected']
    for path, expected in protected.items():
        if expected != 'UNAVAILABLE':
            assert digest(pathlib.Path(path)) == expected, path
    manifest = json.loads((ROOT / 'build/hardware-20261023/input-manifest.json').read_text(encoding='utf-8'))
    for path, entry in manifest.items():
        actual = ROOT / 'logs/2026.10.23' / path
        assert actual.stat().st_size == entry['bytes'] and digest(actual) == entry['sha256'], path
    package = ROOT / 'windows/Space Rangers HD A War Apart/DATA/robots.pkg'
    assert digest(package) == results['host']['package_sha256'] == results['arm64']['package_sha256']
    for name, expected in results['host']['source_cfg_sha256'].items():
        assert digest(ROOT / 'upstream/matrixgame/MatrixGame/CFG' / name) == expected
    paths = [p for folder in ('original-init-host', 'original-init-arm64', 'original-init-sdk')
             for p in (base / folder).iterdir() if p.is_file() and p.suffix in ('.o', '.a', '.elf')]
    paths += [base / ('original-init-' + kind) / 'original-game-init' for kind in ('host', 'arm64')]
    tracked = subprocess.check_output([git, 'diff', '--name-only', '--ignore-submodules'], cwd=ROOT, text=True).splitlines()
    tracked += ['port/matrixgame/native-engine/original_game.cpp', 'port/matrixgame/native-engine/original_game.hpp',
                'tests/test_matrixgame_original_init.cpp', 'tools/matrixgame_original_game_overrides.py']
    evidence = dict(parent=parent, original_pin='218115a72fa9c5d43269f4ca84a8596a0bcb82f1',
                    source_fidelity=dict(whole_MatrixGame_inverse_three_hooks=True, original_static_init_identical=True,
                                         effective_header_differences=header_diffs, runtime_member_layout_identical=True),
                    source_sha256={p: digest(ROOT / p) for p in tracked},
                    artifact_sha256={str(p.relative_to(ROOT)): digest(p) for p in paths},
                    results=results, sdk=sdk, msvc=msvc, protected=protected,
                    hardware_files_verified=len(manifest), hardware_runtime=False, successful_game_init=False)
    output = ROOT / 'reports/matrixgame-e2e519-original-init-evidence.json'
    output.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'fidelity': evidence['source_fidelity'], 'hardware_files_verified': len(manifest),
                      'protected_available': sum(v != 'UNAVAILABLE' for v in protected.values()),
                      'host_and_ARM64_actual_Init': True, 'SDK_link': True}, ensure_ascii=False))


if __name__ == '__main__':
    main()
