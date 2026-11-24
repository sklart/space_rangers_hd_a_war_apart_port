"""Read-only comparison of the frozen E2E516 baseline and integration inputs.

This does not rebuild, modify the baseline or claim runtime equivalence.
"""
import hashlib
import json
import subprocess
import argparse
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUNDLE = ROOT / 'build/e2e516/hardware-e2e516-03651ce.zip'
ARCHIVE_REPORT = ROOT / 'build/matrixgame/integrated-grmain-sdk-20261124-1/result.json'
INTEGRATED_ELF = ROOT / 'build/matrixgame/integrated-app-20261123/matrixgame-integrated-app-ui720.elf'
OUTPUT = ROOT / 'build/matrixgame/e2e516-integrated-audit-20261124-1.json'
NM = Path('C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe')

def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=OUTPUT)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to((ROOT / 'build').resolve()):
        raise ValueError('Audit output must stay under build/')
    provenance = json.loads((ROOT / 'reports/e2e5/e2e516-artifact-provenance.json').read_text(encoding='utf-8'))
    if sha(BUNDLE) != provenance['bundle']['sha256']:
        raise RuntimeError('Frozen E2E516 bundle hash differs from provenance')
    with zipfile.ZipFile(BUNDLE) as z:
        if z.testzip() is not None:
            raise RuntimeError('Frozen E2E516 bundle failed CRC')
        inputs = json.loads(z.read('source-sha256.json'))
        mapping = json.loads(z.read('effective-source-map.json'))
        manifest = json.loads(z.read('manifest.json'))
    sources = {'identical': [], 'changed': [], 'missing': []}
    for name, expected in inputs.items():
        path = ROOT / name
        category = 'missing' if not path.is_file() else 'identical' if sha(path) == expected.lower() else 'changed'
        sources[category].append(name)
    report = json.loads(ARCHIVE_REPORT.read_text(encoding='utf-8'))
    archive = Path(report['archive'])
    if sha(archive) != report['archive_sha256']:
        raise RuntimeError('Integration archive differs from its recorded hash')
    members = report['archive_members_sha256']
    link = ROOT / 'port/switch/build/e2e-game-perf515/link-objects.rsp'
    objects = {'identical': [], 'different': [], 'missing_baseline': [], 'excluded': []}
    for line in link.read_text(encoding='utf-8').splitlines():
        if not line.strip():
            continue
        path = Path(line.strip().strip('"'))
        if path.name not in members:
            category = 'excluded'
        elif not path.is_file():
            category = 'missing_baseline'
        elif sha(path) == members[path.name]:
            category = 'identical'
        else:
            category = 'different'
        objects[category].append(str(path))
    result = subprocess.run([str(NM), '-C', '--defined-only', str(INTEGRATED_ELF)],
                            check=True, capture_output=True, text=True, encoding='utf-8', errors='replace')
    symbols = result.stdout
    probes = ['Rangers::ProgramMain()', 'RunInitialRangersScreens()',
              'controller_ui::Process(', 'input_platform::NormalizeAt(',
              'NativeRangersInterface::Run(', 'renderer_platform::PresentRgb565(']
    result = {
        'status': 'INVENTORY ONLY; integration equivalence and Switch runtime NOT VERIFIED',
        'bundle_sha256': sha(BUNDLE), 'baseline_build_id': manifest['build_id'],
        'baseline_source_commit': provenance['source_commit'],
        'integrated_elf_sha256': sha(INTEGRATED_ELF), 'integrated_archive_sha256': sha(archive),
        'source_counts': {k: len(v) for k, v in sources.items()}, 'sources': sources,
        'object_counts': {k: len(v) for k, v in objects.items()}, 'objects': objects,
        'selected_effective_sources': {k: v for k, v in mapping.items() if Path(k).name in
            {'program.cpp', 'Rangers.cpp', 'GR_Main.cpp', 'GI_MessageLoop.cpp', 'fLoad.cpp', 'ab_StopLine.cpp'}},
        'integrated_symbol_presence': {p: p in symbols for p in probes},
    }
    if output.exists():
        raise RuntimeError('Audit output already exists')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(json.dumps({k: result[k] for k in ('status', 'source_counts', 'object_counts', 'integrated_symbol_presence')}, indent=2))

if __name__ == '__main__':
    main()
