"""Recheck current UI text without rewriting historical evidence or building NROs."""
import json
import subprocess
from pathlib import Path
from matrixgame_text_checks import sha, SOURCES

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/matrixgame/e2e538-selection/text-recheck'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    profiles = {}
    for kind in ('host', 'arm64'):
        result = read(BASE / kind / 'result.json')
        assert result['status'] == 'PASS', kind
        assert all(c['exit'] == 0 for c in result['commands']), kind
        assert 'glyph/layout goldens=17940;' in result['stdout'], kind
        assert 'CPU_UPLOAD_MODEL_ONLY SWITCH_HARDWARE_NOT_RUN' in result['stdout']
        # text_checks also records ancillary files (including Switch code that
        # HOST never compiles). Verify actual compilation inputs here; the final
        # Switch recorder is checked against the fresh SDK manifest below.
        for name in SOURCES + ['port/matrixgame/native-engine/native_text.hpp']:
            digest = result['source_sha256'][name]
            assert sha(ROOT / name) == digest, (kind, name)
        for name, digest in result['assets_sha256'].items():
            assert sha(ROOT / name) == digest, (kind, name)
        assert sha(BASE / kind / 'native-text') == result['binary_sha256']
        profiles[kind] = result
    assert '-fsanitize=address,undefined' in profiles['host']['commands'][0]['command']
    windows = read(ROOT / 'build/matrixgame/e2e524-text/windows/result.json')
    assert windows['status'] == 'PASS'
    for name in ('native_text.cpp', 'native_text.hpp'):
        name = 'port/matrixgame/native-engine/' + name
        assert sha(ROOT / name) == windows['source_sha256'][name], name
    for name, digest in windows['assets_sha256'].items():
        assert sha(ROOT / name) == digest, name
    assert sha(ROOT / 'build/matrixgame/e2e524-text/windows/font0.sfnt') == sha(ROOT / 'upstream/matrixgame/MatrixGame/Rangers.ttf')
    sdk_root = ROOT / 'build/matrixgame/e2e538-selection/sdk'
    sdk = read(sdk_root / 'result.json')
    assert sdk['status'] == 'COMPILE/LINK PASS; SWITCH RUNTIME NOT RUN'
    assert not sdk['strong_undefined']
    assert sha(sdk_root / 'matrixgame-native-entry.elf') == sdk['elf_sha256']
    for name, digest in sdk['source_sha256'].items():
        assert sha(ROOT / name) == digest, name
    for name, digest in sdk['link_inputs_sha256'].items():
        assert sha(ROOT / name) == digest, name
    symbols = (sdk_root / 'symbols.log').read_text(encoding='utf-8')
    for symbol in ('Text::Render(', 'text_backend::DeviceIdentity()', 'text_backend::native::Draw(', 'text_backend::native::OriginalAsset('):
        assert symbol in symbols, symbol
    previous = read(ROOT / 'reports/matrixgame-e2e521-font-evidence.json')
    protected = 0
    for name, value in previous['protected'].items():
        if value.get('status') != 'UNAVAILABLE':
            assert sha(Path(name)) == value['sha256'], name
            protected += 1
    for name, value in previous['extra_frozen_artifacts_verified'].items():
        assert sha(Path(name)) == value['sha256'], name
    primary = previous['primary_NRO_verified_against_previous_fingerprint']
    assert sha(Path(primary['path'])) == primary['sha256']
    assert sha(ROOT / 'Switch-E2E5-TEST.nro') == previous['root_cumulative_NRO_verified_against_documented_hash']
    hardware = read(ROOT / 'build/hardware-20261023/input-manifest.json')
    for name, value in hardware.items():
        assert sha(ROOT / 'logs/2026.10.23' / name) == value['sha256'], name
    assert sha(ROOT / 'windows/Space Rangers HD A War Apart/DATA/robots.pkg') == '8a22f856a27a8937ef28ac61274ffdb85cc2ff865492bd4d3ddeb2a6054011f0'
    for name, digest in read(ROOT / 'build/matrixgame/e2e519/original-init-host/result.json')['source_cfg_sha256'].items():
        assert sha(ROOT / 'upstream/matrixgame/MatrixGame/CFG' / name) == digest, name
    frozen = read(ROOT / 'reports/e2e5/hardware-20261023-cache-frozen-audit.json')
    assert sha(ROOT / frozen['cache_source']) == frozen['expected_sha256']
    git = r'C:\Program Files\Git\cmd\git.exe'
    def git_read(*args, cwd=ROOT):
        return subprocess.check_output([git, *args], cwd=cwd, text=True).strip()
    assert git_read('rev-parse', 'HEAD') == previous['parent']
    assert git_read('rev-parse', 'HEAD', cwd=ROOT / 'upstream/matrixgame') == '218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
    assert not git_read('status', '--porcelain', '--untracked-files=all', cwd=ROOT / 'upstream/matrixgame')
    assert not list(BASE.rglob('*.nro'))
    result = dict(status='TEXT SOFTWARE RECHECK PASS; NATIVE INIT HARDWARE PENDING',
                  fresh_profiles=profiles, windows_oracle='E2E-5.24 reused; unchanged rasterizer and glyph assets; 17940 exact cases',
                  sdk='E2E-5.38 fresh full SDK compile/link; artifact/link-input/source hashes and required text symbols verified',
                  sdk_elf_sha256=sdk['elf_sha256'], sdk_link_inputs=len(sdk['link_inputs_sha256']),
                  protected_verified=protected, hardware_inputs_verified=len(hardware),
                  Switch_hardware='NOT RUN / HARDWARE_PENDING', native_map_loaded=False,
                  native_frame_presented=False, NRO_created=False, commit=False, push=False,
                  fire_alias_baseline='RunOriginalSelectionScenario: releasing pad fire key interrupted mouse-owned original fire',
                  open_gates=['P0', 'GI repeating-text causality unproven', 'manual-publish ENOENT', 'Switch hangs', 'E2E-5.34 raw HAL retry FAIL'])
    (ROOT / 'reports/matrixgame-text-current-evidence.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('status', 'protected_verified', 'hardware_inputs_verified', 'sdk_link_inputs')}, ensure_ascii=False))


if __name__ == '__main__':
    main()
