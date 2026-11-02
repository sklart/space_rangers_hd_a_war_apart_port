"""Verify complete original ReadParams CPU evidence without rebuilding the NRO."""
import argparse
import json
import subprocess
from pathlib import Path

from matrixgame_config_tail_checks import BASE, ENGINE, DATA, cache
from matrixgame_standard_nro_audit import ROOT, read, sha, check_hashes


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    profiles = {}
    for kind in ('host', 'arm64', 'windows'):
        directory = BASE / kind
        result = read(directory / 'result.json')
        assert result['status'] == 'PASS; FULL ORIGINAL ReadParams CPU ONLY', kind
        assert result['commands'] and all(c['exit'] == 0 for c in result['commands']), kind
        assert result['source_sha256_before'] == result['source_sha256_after']
        check_hashes(result['source_sha256_after'])
        assert result['data_txt_sha256_before'] == result['data_txt_sha256_after'] == sha(DATA)
        check_hashes(result['generated_input_sha256'], ENGINE)
        if kind == 'host':
            build = directory / 'build-native'
            flags = (build / 'CMakeFiles/matrixgame-original-config-contract.dir/flags.make').read_text()
            assert '-fsanitize=address,undefined' in flags and '-UNDEBUG' in flags
        elif kind == 'arm64':
            build = directory / 'build-make'
            assert 'qemu-aarch64' in next(c['command'] for c in result['commands'] if c['label'] == 'runtime')
        else:
            build = directory / 'build'
        exe = build / ('Release/matrixgame-original-config-contract.exe' if kind == 'windows' else 'matrixgame-original-config-contract')
        assert sha(exe) == result['binary_sha256']
        assert sha(directory / 'observed.bin') == result['observations_sha256']
        assert 'ORIGINAL FULL ReadParams returned=4' in result['stdout']
        assert '25 labels + 25 descriptions' in result['stdout']
        assert 'CPU_GAMMA_ONLY NO_NATIVE_INIT NO_MAP NO_GPU' in result['stdout']
        # Hash actual codec binaries/config headers used by this profile. Host
        # shared objects live in WSL; ask that local interpreter to read them.
        cfg = cache(build / 'CMakeCache.txt')
        dependency_names = ('ZLIB_LIBRARY', 'PNG_LIBRARY_RELEASE', 'JPEG_LIBRARY_RELEASE')
        dependencies = {key: cfg.get(key) or cfg.get(key + '_RELEASE') for key in dependency_names}
        if kind == 'windows':
            result['dependency_binary_sha256_at_audit'] = {name: sha(Path(path)) for name, path in dependencies.items()}
        else:
            code = 'import hashlib,json;paths=' + repr(dependencies) + ';print(json.dumps({k:hashlib.sha256(open(p,"rb").read()).hexdigest() for k,p in paths.items()}))'
            result['dependency_binary_sha256_at_audit'] = json.loads(subprocess.check_output(['wsl', '--exec', '/usr/bin/python3', '-c', code], text=True))
        profiles[kind] = result
    digests = {result['observations_sha256'] for result in profiles.values()}
    assert len(digests) == 1, 'Cross-platform config observations differ'
    previous = read(ROOT / 'reports/matrixgame-e2e558-gamma-evidence.json')
    check_hashes(previous['metadata']['own_source_sha256'])
    nro = previous['NRO']
    assert sha(ROOT / nro['path']) == nro['sha256']
    baseline = read(ROOT / 'reports/matrixgame-e2e556-standard-nro-evidence.json')
    check_hashes(baseline['protected_verified'])
    assert sha(Path(baseline['primary_NRO']['path'])) == baseline['primary_NRO']['sha256']
    assert sha(ROOT / baseline['nro_path']) == baseline['nro_sha256']
    for directory, entries in baseline['previous_diagnostic_packages_preserved'].items():
        check_hashes(entries, ROOT / 'dist' / directory)
    frozen = read(ROOT / 'reports/matrixgame-e2e521-font-evidence.json')
    check_hashes({name: value['sha256'] for name, value in frozen['extra_frozen_artifacts_verified'].items()})
    assert sha(ROOT / 'Switch-E2E5-TEST.nro') == frozen['root_cumulative_NRO_verified_against_documented_hash']
    assert sha(ROOT / 'windows/Space Rangers HD A War Apart/DATA/robots.pkg') == '8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
    cfg_inputs = read(ROOT / 'build/matrixgame/e2e519/original-init-host/result.json')['source_cfg_sha256']
    check_hashes(cfg_inputs, ROOT / 'upstream/matrixgame/MatrixGame/CFG')
    hardware = read(ROOT / 'build/hardware-20261023/input-manifest.json')
    check_hashes({name: value['sha256'] for name, value in hardware.items()}, ROOT / 'logs/2026.10.23')
    user_log = read(ROOT / 'reports/matrixgame-e2e557-logs2-evidence.json')
    assert sha(Path(user_log['archive_path'])) == user_log['archive_sha256']
    check_hashes(user_log['inputs_sha256'])
    git = r'C:\Program Files\Git\cmd\git.exe'
    upstream = ROOT / 'upstream/matrixgame'
    assert subprocess.check_output([git, '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip() == baseline['upstream_revision']
    assert not subprocess.check_output([git, '-C', str(upstream), 'status', '--porcelain'], text=True)
    assert subprocess.check_output([git, 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip() == previous['metadata']['parent']
    output = {'status': 'FULL ORIGINAL ReadParams CPU PASS; NATIVE MAP/INIT HARDWARE PENDING',
        'profiles': profiles, 'cross_platform_observations_sha256': next(iter(digests)),
        'actual_caller_of_first_gamma': 'CGame::Init -> g_Config.ReadParams -> ApplyGammaRamp',
        'ApplySettings_calls_gamma': False, 'original_config_TU_sha256': sha(ENGINE / 'MatrixGame/src/MatrixConfig.cpp'),
        'all_E558_production_source_inputs_unchanged': True, 'E558_NRO_preserved': nro,
        'protected_verified': len(baseline['protected_verified']), 'hardware_inputs_verified': len(hardware),
        'input_data_txt_sha256': sha(DATA), 'full_native_Init_executed': False,
        'GPU_provider_fixture': 'Gamma-only routing; ReadCaps/AvailableTextureBytes explicitly Unsupported',
        'new_NRO_created': False, 'Switch_hardware': 'NOT RUN FOR E558/E559', 'commit': False, 'push': False}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(output, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    print(json.dumps({key: output[key] for key in ('status', 'cross_platform_observations_sha256', 'all_E558_production_source_inputs_unchanged', 'new_NRO_created')}, indent=2))


if __name__ == '__main__':
    main()
