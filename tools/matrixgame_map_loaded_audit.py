"""Audit existing scoped runs and protected inputs; do not rerun large suites."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/e2e519'


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    results = {}
    files = {}
    def capture(path):
        assert path.is_file(), path
        files[path.relative_to(ROOT).as_posix()] = digest(path)

    for label, filename in {
        'ledger_host': 'skin-borrow-assertions-host/result.json',
        'ledger_arm64': 'skin-borrow-assertions-arm64/result.json',
        'vo_host': 'skin-borrow-vo-assertions-host/result.json',
        'vo_arm64': 'skin-borrow-vo-assertions-arm64/result.json',
        'ledger_windows': 'skin-borrow-windows/result.json',
        'vo_windows': 'skin-borrow-windows/vo-result.json',
        'sdk': 'skin-borrow-sdk/result.json',
        'windows_map': 'skin-borrow-windows/provider-result.json',
    }.items():
        path = BASE / filename
        value = read(path)
        assert all(v == 0 for v in value['results'].values()), label
        if label not in ('windows_map', 'sdk'):
            assert value['runtime_exit'] == 0 and not value['stderr'], label
        results[label] = value
        capture(path)
        if label.endswith(('host', 'arm64')):
            for cmd in value['commands']:
                if '-c' in cmd:
                    flags = [x for x in cmd if x in ('-DNDEBUG', '-UNDEBUG')]
                    assert flags[-1] == '-UNDEBUG', (label, flags)

    path_results = {}
    for label, name in (
        ('host', 'path-candidate-host/result.json'),
        ('arm64', 'path-candidate-arm64/result.json'),
        ('sdk', 'path-sdk-assertions/result.json'),
        ('windows', 'full-windows-candidate/path-runtime.json'),
    ):
        path = BASE / name
        value = read(path)
        capture(path)
        if label == 'sdk':
            assert all(x == 0 for x in value['results'].values())
            assert not value['strong_undefined'] and value['runtime'] is False
            for cmd in value['commands']:
                if isinstance(cmd, list) and '-c' in cmd:
                    assert cmd[-1] == '-UNDEBUG'
        else:
            assert value['runtime_exit'] == 0
            assert 'moves=6' in value['stdout']
        path_results[label] = value
    # Existing host/ARM64/Windows binaries still match the earlier audit.
    historical = read(ROOT / 'reports/matrixgame-e2e519-path-full-windows-evidence.json')
    for name, expected in historical['artifact_sha256'].items():
        if any(x in name for x in ('/path-candidate-host/', '/path-candidate-arm64/', '/full-windows-candidate/')):
            assert digest(ROOT / name) == expected, name
            capture(ROOT / name)
    assert digest(ROOT / 'tests/test_matrixgame_path_effect.cpp') == historical['source_sha256']['tests/test_matrixgame_path_effect.cpp']
    assert digest(ROOT / 'tools/matrixgame_effects_overrides.py') == historical['source_sha256']['tools/matrixgame_effects_overrides.py']
    capture(ROOT / 'tests/test_matrixgame_path_effect.cpp')

    sdk = results['sdk']
    assert sdk['runtime'] is False and sdk['full_game_link'] is False
    assert not sdk['strong_undefined']
    assert len(sdk['results']) == 12  # Ten TUs, ledger ELF link, nm.
    for cmd in sdk['commands']:
        if '-c' in cmd:
            assert cmd[-1] == '-UNDEBUG', cmd

    windows = results['windows_map']
    assert windows['runtime_exit'] == 10  # Incomplete Init is never a PASS.
    assert 'D3D9 HAL CreateDevice HRESULT=00000000' in windows['stdout']
    assert 'actual SDK material/viewport/light/register readback and restoration PASS' in windows['stdout']
    for line in (
        'original borrowed skin leases empty after cleanup',
        'actual gamma readback restored; device/timer leases empty',
        'original Init stopped; after cleanup map=00000000 heap=00000000',
        'original Init boundary: window resize requires reset-safe presentation ownership',
    ):
        assert windows['stdout'].count(line) == 2, line
    for line in (
        'RobotPreload complete borrowed_skin_objects=0',
        'map storage loaded borrowed_skin_objects=0',
        'PrepareMap complete borrowed_skin_objects=9 map_size=276x285 groups=28x29',
    ):
        assert windows['stderr'].count(line) == 2, line

    # Locate this EXE's actual exception stack, accounting for ASLR.
    out = BASE / 'skin-borrow-windows'
    mapping = (out / 'windows-game.map').read_text()
    preferred = int(re.search(r'Preferred load address is ([0-9a-fA-F]+)', mapping)[1], 16)
    symbols = []
    for line in mapping.splitlines():
        found = re.match(r'\s*[0-9A-Fa-f]+:[0-9A-Fa-f]+\s+(\S+)\s+([0-9A-Fa-f]{8})\s', line)
        if found:
            symbols.append((int(found[2], 16), found[1]))
    match = re.search(r'exception code=e06d7363 address=[0-9A-Fa-f]+ stack image=([0-9A-Fa-f]+):([^\n]+)', windows['stdout'])
    image = int(match[1], 16)
    stack = []
    for raw in match[2].split():
        address = int(raw, 16) - image + preferred
        if not preferred <= address < preferred + 0x1000000:
            continue
        symbol = max((x for x in symbols if x[0] <= address), default=(0, 'unknown'))
        stack.append({'symbol': symbol[1], 'offset': address - symbol[0]})
    assert any('Init@CGame' in x['symbol'] for x in stack), stack
    source = BASE / 'skin-borrow-source'
    game = (source / 'MatrixGame/src/MatrixGame.cpp').read_text(encoding='utf-8')
    from matrixgame_vector_object_overrides import function
    apply_video = function(game, 'void CGame::ApplyVideoParams(')
    assert 'ResizeOuter(' in apply_video
    assert 'window resize requires reset-safe presentation ownership' in (
        ROOT / 'port/matrixgame/native-engine/platform_3g.hpp').read_text()
    # Stage logging must preserve the previous complete Init algorithm.
    previous = (BASE / 'full-windows-source/MatrixGame/src/MatrixGame.cpp')
    if not previous.is_file():
        previous = BASE / 'path-candidate-source/MatrixGame/src/MatrixGame.cpp'
    before = function(previous.read_text(encoding='utf-8'), 'void CGame::Init(')
    after = function(game, 'void CGame::Init(')
    for stage in ('RobotPreload complete', 'PrepareMap complete'):
        after = after.replace('\n    original_game_owner::ObserveStage("' + stage + '");', '')
        # PrepareMap observation precedes its following source line.
        after = after.replace('    original_game_owner::ObserveStage("' + stage + '");\n', '')
    after = after.replace('original_game_owner::ObserveStage(stor.Load(mapname.c_str()) ? "map storage loaded" : "map storage Load FAILED");',
                          'stor.Load(mapname.c_str());')
    assert after == before, 'Init algorithm changed beyond stage observations'

    # Reprepare into a separate directory to detect drift from the tested view.
    from matrixgame_engine_prepare import prepare_engine
    fresh = BASE / 'map-loaded-audit-source'
    prepare_engine(fresh)
    generated = {}
    for path in source.rglob('*'):
        if path.is_file() and path.suffix in ('.cpp', '.h', '.hpp', '.inl', '.inc'):
            name = path.relative_to(source).as_posix()
            assert digest(path) == digest(fresh / name), name
            generated[name] = digest(path)
    for name in ('VectorObjectCpu.hpp', 'VectorObjectShared.hpp'):
        assert digest(source / 'portable-core' / name) == digest(
            BASE / 'original-init-windows/core/source' / name), name
    # The independently tested Path body is also the one in this full candidate.
    path_name = 'MatrixGame/src/Effects/MatrixEffectPath.cpp'
    assert digest(source / path_name) == digest(BASE / 'path-candidate-source' / path_name)

    for folder in ('skin-borrow-assertions-host', 'skin-borrow-assertions-arm64', 'skin-borrow-vo-assertions-host',
                   'skin-borrow-vo-assertions-arm64', 'skin-borrow-sdk', 'skin-borrow-windows', 'path-sdk-assertions'):
        for path in (BASE / folder).iterdir():
            if path.is_file() and (path.suffix in ('.o', '.obj', '.elf', '.exe', '.map', '.py')
                                   or path.name in ('borrowed-skin', 'borrowed-skin-vo')):
                capture(path)
    # Capture exact runners, generated sources and all native source inputs as
    # they stand at this audit. Command records remain the original build records.
    for path in (ROOT / 'port/matrixgame/native-core').rglob('*'):
        if path.is_file() and path.suffix in ('.cpp', '.hpp', '.inc'):
            capture(path)
    for path in (ROOT / 'port/matrixgame/native-engine').rglob('*'):
        if path.is_file() and path.suffix in ('.cpp', '.hpp', '.inc', '.txt'):
            capture(path)
    for path in (ROOT / 'tools').glob('matrixgame_*.py'):
        capture(path)
    for name in ('test_matrixgame_borrowed_skin.cpp', 'test_matrixgame_borrowed_skin_vo.cpp',
                 'matrixgame_windows_game_main.cpp', 'matrixgame_windows_game_init.cpp'):
        capture(ROOT / 'tests' / name)
    for path in BASE.glob('check-borrowed-skin*.py'):
        capture(path)
    capture(BASE / 'check-path-sdk-assertions.py')
    capture(BASE.parent / 'e2e517/runtime-final/model-0.vo')
    # Resolve reused archive inputs against the actual working directories.
    for label, value in results.items():
        commands = value['commands']
        if isinstance(commands, dict):
            for command in commands.values():
                for name in re.findall(r'"([^"\n]+\.(?:lib|obj))"', command):
                    path = Path(name)
                    if not path.is_absolute():
                        path = BASE / 'original-init-windows' / path
                    if path.is_file() and path.is_relative_to(ROOT):
                        capture(path)
        else:
            for command in commands:
                for name in command:
                    if name.endswith('.a'):
                        path = Path(name.replace('/mnt/d/', 'D:/'))
                        if not path.is_absolute():
                            kind = 'arm64' if label.endswith('arm64') else 'host'
                            path = BASE / ('vector-group-fresh-' + kind) / path
                        if path.is_file() and path.is_relative_to(ROOT):
                            capture(path)

    protected = read(ROOT / 'reports/matrixgame-e2e519-original-init-evidence.json')['protected']
    verified = {}
    for name, expected in protected.items():
        if expected == 'UNAVAILABLE':
            verified[name] = {'status': 'UNAVAILABLE'}
        else:
            assert digest(Path(name)) == expected, name
            verified[name] = {'sha256': expected, 'unchanged': True}
    hardware = read(ROOT / 'build/hardware-20261023/input-manifest.json')
    for name, entry in hardware.items():
        path = ROOT / 'logs/2026.10.23' / name
        assert path.stat().st_size == entry['bytes'] and digest(path) == entry['sha256'], name
    original = read(BASE / 'original-init-host/result.json')
    assert digest(ROOT / 'windows/Space Rangers HD A War Apart/DATA/robots.pkg') == original['package_sha256']
    for name, expected in original['source_cfg_sha256'].items():
        assert digest(ROOT / 'upstream/matrixgame/MatrixGame/CFG' / name) == expected, name
    frozen = read(ROOT / 'reports/e2e5/hardware-20261023-cache-frozen-audit.json')
    assert digest(ROOT / frozen['cache_source']) == frozen['expected_sha256']
    assert all(digest(ROOT / x['file']) == read(ROOT / 'build/e2e516/hardware-e2e516-03651ce/source-sha256.json')[x['file']]
               for x in frozen['evict_calls'])
    parent = subprocess.check_output([r'C:\Program Files\Git\cmd\git.exe', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    cmake_dir = BASE / 'original-init-windows'
    targets = ('matrixgame-native-bootstrap', 'matrixgame-windows-game', 'matrixgame-native-path-effect',
               'matrixgame-native-borrowed-skin', 'matrixgame-native-borrowed-skin-vo')
    for name in targets:
        capture(cmake_dir / (name + '.vcxproj'))
    ctest = (cmake_dir / 'CTestTestfile.cmake').read_text()
    assert all(name in ctest for name in targets[2:])
    capture(cmake_dir / 'CTestTestfile.cmake')
    report = {
        'date': '2026-10-25', 'parent': parent, 'stage': 'PARTIAL',
        'results': results, 'path_results': path_results, 'resolved_exception_stack': stack,
        'CMake_generated_targets_checked': targets,
        'first_blocker': 'CGame::Init -> CGame::ApplyVideoParams -> Window::ResizeOuter: reset-safe presentation ownership',
        'HAL_created': True, 'storage_loaded': True, 'PrepareMap_returned': True,
        'map_size': [276, 285], 'group_size': [28, 29], 'borrowed_VOs': 9,
        'clean_attempts': 2, 'Init_returned': False, 'first_frame': False,
        'game_loop_executed': False, 'Switch_runtime': False,
        'generated_source_reprepared_and_equal': generated,
        'VO_class_headers_equal_to_reused_windows_cache': True,
        'Init_algorithm_equal_after_removing_stage_observations': True,
        'file_sha256_at_audit': files, 'protected': verified,
        'hardware_files_verified': len(hardware), 'original_package_and_CFG_unchanged': True,
        'frozen_GI_defect_applicability': 'source/resource applicable; screen occurrence and repeated-text causality OPEN',
        'manual_publish_ENOENT': 'OPEN', 'Switch_hangs': 'OPEN',
        'commit': False, 'push': False,
    }
    target = ROOT / 'reports/matrixgame-e2e519-map-loaded-evidence.json'
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('stage', 'HAL_created', 'PrepareMap_returned',
        'map_size', 'clean_attempts', 'Init_returned', 'first_blocker', 'hardware_files_verified')}, ensure_ascii=False))


if __name__ == '__main__':
    main()
