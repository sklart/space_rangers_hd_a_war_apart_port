"""Audit the gamma candidate, its exact saved checks, and protected inputs."""
import argparse
import json
import subprocess
from pathlib import Path

from matrixgame_standard_nro_audit import ROOT, read, sha, check_hashes
from matrixgame_diagnostic_package import verify_nro

BASE = ROOT / 'build/matrixgame/e2e558-gamma'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    profiles = {}
    for kind in ('host', 'arm64', 'windows', 'gl'):
        directory = BASE / kind
        result = read(directory / 'result.json')
        assert result['status'].startswith('PASS;'), kind
        assert result['commands'] and all(c['exit'] == 0 for c in result['commands']), kind
        assert result['source_sha256_before'] == result['source_sha256_after']
        check_hashes(result['source_sha256_after'])
        assert sha(directory / 'original-gamma.inc') == result['extracted_source_sha256']
        exe = directory / ('gamma.exe' if kind == 'windows' else 'gamma-test')
        assert sha(exe) == result['binary_sha256']
        if kind != 'gl':
            assert sha(directory / 'ramps.bin') == result['ramps_sha256']
            assert 'ORIGINAL gamma calls=18; production LUT samples=4128768;' in result['stdout']
        else:
            assert 'ramps=21 pixels=69888' in result['stdout']
        profiles[kind] = result
    assert len({profiles[k]['ramps_sha256'] for k in ('host', 'arm64', 'windows')}) == 1
    assert profiles['gl']['input_ramps_sha256'] == profiles['host']['ramps_sha256']

    sdk = BASE / 'sdk'
    metadata = read(sdk / 'native-diagnostic-build.json')
    check_hashes(metadata['own_source_sha256'])
    check_hashes(metadata['generated_original_sha256'], sdk / 'source')
    prior = read(ROOT / 'reports/matrixgame-e2e556-standard-nro-evidence.json')
    before = prior['metadata']
    assert metadata['generated_original_sha256'] == before['generated_original_sha256']
    changes = {name for name, digest in before['own_source_sha256'].items() if sha(ROOT / name) != digest}
    assert changes == {'port/matrixgame/deko/materials.cmake',
        'port/matrixgame/native-engine/platform_switch.cpp', 'port/matrixgame/native-engine/platform_switch.hpp',
        'port/matrixgame/native-engine/native_diagnostic_pixels.hpp', 'port/matrixgame/native-engine/native_diagnostic_smoke.cpp'}, changes
    additions = set(metadata['own_source_sha256']) - set(before['own_source_sha256'])
    assert additions == {'port/matrixgame/deko/gamma_program.cpp', 'port/matrixgame/deko/gamma_program.hpp',
        'port/matrixgame/deko/gamma.vert.glsl', 'port/matrixgame/deko/gamma.frag.glsl',
        'port/matrixgame/native-engine/native_gamma.hpp', 'tools/matrixgame_gamma_audit.py',
        'tools/matrixgame_gamma_checks.py', 'tools/matrixgame_gamma_shader_build.py'}, additions
    shaders = read(sdk / 'material-shaders/gamma-shader-build.json')
    elf = sdk / 'matrixgame-native-diagnostic.elf'
    image = elf.read_bytes()
    for stage, entry in shaders.items():
        source = ROOT / f'port/matrixgame/deko/gamma.{stage}.glsl'
        compiled = sdk / f'material-shaders/gamma.{stage}.dksh'
        assert sha(source) == entry['source_sha256'] and sha(compiled) == entry['dksh_sha256']
        assert compiled.read_bytes() in image
    symbols = subprocess.check_output(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe', '-C', str(elf)], text=True)
    assert not [line for line in symbols.splitlines() if ' U ' in line]
    for symbol in ('CGame::Init(', 'DekoDevice::ApplyCalibratedGamma(', 'DekoGammaProgram::Record(', 'DekoDevice::RequestPresentationReadback('):
        assert symbol in symbols, symbol
    package = sdk / 'diagnostic-nro'
    nro = package / 'matrixgame-native-diagnostic.nro'
    validation = verify_nro(nro.read_bytes(), Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(), (package / 'control.nacp').read_bytes())
    check_hashes(prior['protected_verified'])
    assert sha(ROOT / prior['nro_path']) == prior['nro_sha256']
    assert sha(Path(prior['primary_NRO']['path'])) == prior['primary_NRO']['sha256']
    for name, entries in prior['previous_diagnostic_packages_preserved'].items():
        check_hashes(entries, ROOT / 'dist' / name)
    hardware = read(ROOT / 'build/hardware-20261023/input-manifest.json')
    check_hashes({name: entry['sha256'] for name, entry in hardware.items()}, ROOT / 'logs/2026.10.23')
    frozen = read(ROOT / 'reports/matrixgame-e2e521-font-evidence.json')
    check_hashes({name: entry['sha256'] for name, entry in frozen['extra_frozen_artifacts_verified'].items()})
    assert sha(ROOT / 'Switch-E2E5-TEST.nro') == frozen['root_cumulative_NRO_verified_against_documented_hash']
    assert sha(ROOT / 'windows/Space Rangers HD A War Apart/DATA/robots.pkg') == '8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
    cfg = read(ROOT / 'build/matrixgame/e2e519/original-init-host/result.json')['source_cfg_sha256']
    check_hashes(cfg, ROOT / 'upstream/matrixgame/MatrixGame/CFG')
    user_log = read(ROOT / 'reports/matrixgame-e2e557-logs2-evidence.json')
    assert sha(Path(user_log['archive_path'])) == user_log['archive_sha256']
    check_hashes(user_log['inputs_sha256'])
    git = r'C:\Program Files\Git\cmd\git.exe'
    upstream = ROOT / 'upstream/matrixgame'
    assert subprocess.check_output([git, '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip() == prior['upstream_revision']
    assert not subprocess.check_output([git, '-C', str(upstream), 'status', '--porcelain'], text=True)
    assert subprocess.check_output([git, 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip() == metadata['parent']
    output = {'status': 'ORIGINAL GAMMA / PRESENTATION LUT SOFTWARE PASS; NATIVE HARDWARE PENDING',
        'profiles': profiles, 'metadata': metadata, 'source_changes_from_E556': sorted(changes),
        'source_additions_from_E556': sorted(additions), 'generated_original_files_unchanged': len(metadata['generated_original_sha256']),
        'shaders': shaders, 'elf_sha256': sha(elf), 'strong_undefined': [],
        'NRO': {'path': str(nro.relative_to(ROOT)), 'sha256': sha(nro), 'bytes': nro.stat().st_size, 'validation': validation},
        'protected_verified': len(prior['protected_verified']), 'hardware_inputs_verified': len(hardware),
        'E556_NRO_preserved': prior['nro_sha256'], 'previous_packages_preserved': True,
        'Switch_hardware': 'NOT RUN FOR THIS CANDIDATE', 'full_native_Init_executed': False,
        'last_hardware_boundary': 'E557 user logs2: original Init failed in ApplyGammaRamp, before map',
        'physical_display_calibration_claimed': False, 'performance_measured': False, 'commit': False, 'push': False}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(output, stream, ensure_ascii=False, indent=2)
        stream.write('\n')
    print(json.dumps({'status': output['status'], 'NRO': output['NRO'], 'build_id': metadata['build_id']}, indent=2))


if __name__ == '__main__':
    main()
