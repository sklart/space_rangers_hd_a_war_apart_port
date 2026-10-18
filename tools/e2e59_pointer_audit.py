"""Inventory effective pointer conversions without treating every integer as an address."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CAST = re.compile(r"(?:static_cast<std::(?:u?int32_t)>|std::uint32_t)\(reinterpret_cast<std::uintptr_t>\(")
FUNCTION = re.compile(r"^    [\w:<>,*& ]+? ([\w:~]+)\([^;]*\) \{")
NUMERIC = re.compile(r"RangerRelations|AwardIds|RecentlyDroppedItemIds|Damages|Colors|Predecessors|NameLists|Weights|GroupIndex|Sounds->GetCurrentData|Event->Data|ScriptActionTypeStack")


def classify(file, function, line):
    if 'FlyToStar' in line and 'SetInt' in line:
        return 'REVIEW_NATIVE_SEMANTICS', 'ShipDestination Date reads FlyToStar in the pinned reconstruction; verify native field semantics before changing the query.'
    if 'LibraryWord' in line or 'LibraryFunData' in line or 'Signature[1]' in line:
        return 'UNSUPPORTED_WIN32_DLL_ABI', 'The dynamic library adapter explicitly rejects non-Win32-x86 calls; these words are not native game-object storage.'
    if 'IdTo' in line or any(s in function for s in ('LoadFromBuffer', 'Rebind', 'LoadRefs', 'ResolveLoadedReferences')):
        return 'SERIALIZED_ID', 'Read/rebind path temporarily stores a 32-bit on-disk ID in a pointer slot; the value is resolved before object use.'
    if NUMERIC.search(line):
        return 'NUMERIC_PAYLOAD', 'The list or event slot stores numeric values, not dereferenceable object addresses.'
    if function == 'TGalaxy::HideSpecialConstellation' and 'std::bit_cast<float>' in line:
        return 'FLOAT_BITS_PAYLOAD', 'Outline lists are populated with bit_cast<uint32_t>(float); these are coordinate bits, not addresses.'
    if function == 'RunItemUseCode' or file == 'aGalaxyEvent.cpp':
        return 'NUMERIC_PAYLOAD', 'Item-use return values and TGalaxyEvent::AddData are explicitly integer-valued; their list cells are not object pointers.'
    if 'reinterpret_cast<GR_gi::' in line or '&reinterpret_cast<PColor' in line or '&reinterpret_cast<GR_GraphBuf::PColor' in line:
        return 'FORMAT_OFFSET', 'The synthetic pointer is a byte offset into an explicitly sized resource record.'
    if 'AppendLog' in line or 'self=' in line or 'FGBC=' in line or 'FRBC=' in line or 'FIR=' in line:
        return 'DIAGNOSTIC_ADDRESS_ONLY', 'Only diagnostic text loses bits; no address is recovered from this output.'
    if '& 3)' in line:
        return 'ALIGNMENT_LOW_BITS', 'Only the low alignment bits are inspected.'
    if 'GetTargetPoint' in line or 'GetWeaponPortPoint' in line:
        return 'VISUAL_SEED', 'A 32-bit visual seed is derived from addresses; it is never converted back to a pointer.'
    if file.endswith('crc.hpp'):
        return 'CRC_COMPATIBILITY', 'Documented legacy signed 32-bit span comparison, not a reconstructed live address.'
    return 'REQUIRES_REVIEW', 'Producer/consumer semantics are not proven by this lexical inventory.'


def audit(build):
    mapping = json.loads((build / 'effective-source-map.json').read_text())
    paths = {ROOT / path for path in mapping.values()}
    excluded = {'runtime_failure_hooks_portable.cpp', 'runtime_time.cpp',
                'sysutils_imports_portable.cpp', 'windows_imports_portable.cpp'}
    platform = {p for p in (ROOT / 'port/switch/platform').glob('*.cpp') if p.name not in excluded}
    paths.update(platform)
    for directory in (build / 'source-overlay', build / 'runtime-overlay', ROOT / 'port/switch/platform'):
        paths.update(directory.rglob('*.hpp'))
    findings = []
    for path in sorted(paths):
        function = ''
        for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            match = FUNCTION.match(line)
            if match:
                function = match[1]
            if not CAST.search(line):
                continue
            category, reason = classify(path.name, function, line)
            findings.append(dict(file=path.relative_to(ROOT).as_posix(), line=number,
                                 function=function, classification=category, reason=reason,
                                 source=line.strip()))
    failures = []
    for header, forbidden in (
        ('types/aGalaxy.hpp', 'std::uint32_t GateFilmId;'),
        ('types/aGalaxy.hpp', 'std::uint32_t EffectFilmId;'),
        ('types/aGalaxy.hpp', 'std::int32_t FilmObjectId;'),
        ('types/GI_MessageLoop.hpp', 'std::int32_t UserData;'),
        ('types/GI_MessageLoop.hpp', 'pas::Method<void(PCallbackTimerGI, std::int32_t)>'),
        ('types/ab_StopLine.hpp', 'std::int32_t UserValue;'),
        ('types/EC_CacheFont.hpp', 'std::int32_t ObjectId;'),
        ('types/fShip2.hpp', 'std::int32_t DisplayedItemKey;'),
    ):
        if forbidden in (build / 'source-overlay' / header).read_text():
            failures.append(f'{header}: {forbidden}')
    for original in ('fPlanetNO.cpp', 'fGov.cpp', 'fScore.cpp', 'GI_AImage.cpp', 'fStarMap.cpp', 'fEquipmentShop.cpp'):
        source = ROOT / mapping['upstream/cpp/src/' + original]
        if CAST.search(source.read_text()):
            failures.append(f'live-pointer narrowing returned in {original}')
    script = (ROOT / mapping['upstream/cpp/src/aScriptFun.cpp']).read_text()
    if ('e2e5_script_pointer_int::Set(av[0], reinterpret_cast<std::uintptr_t>(ruins->FlyToStar))'
            not in script):
        failures.append('ShipDestination Date must retain the native address with its original int result tag')
    expression = (ROOT / mapping['upstream/cpp/src/EC_Expression.cpp']).read_text()
    if expression.count('srhd_awa::platform::e2e4_script_value::NumericIntFromStorage(') != 7:
        failures.append('Int numeric getters/conversions must retain the original signed-32 view')
    int_setter = expression.split('void TVarEC::SetInt(std::int32_t Value) {', 1)[-1].split('void TVarEC::SetDword(', 1)[0]
    if 'DwordValue = static_cast<std::uint32_t>(Value);' not in int_setter:
        failures.append('SetInt must zero-extend the original DWORD bits in a Dword cell')
    if ('pas::is_one_of<vkInt, vkDword>(TVarEC_RealVType(Source))' not in expression):
        failures.append('Integer assignment must retain native pointer storage')
    return dict(schema=1, level='EFFECTIVE SOURCE INVENTORY; NOT HARDWARE COVERAGE',
                original_translation_units=len(mapping), platform_translation_units=len(platform),
                translation_units=len(mapping) + len(platform), scanned_files=len(paths),
                source_sha256=({original: hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                                for original, path in mapping.items()} |
                               {path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                                for path in sorted(platform)}),
                classifications=dict(Counter(row['classification'] for row in findings)),
                fixed_site_regression_failures=failures, findings=findings)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-root', type=Path, default=ROOT / 'port/switch/build/e2e-game-combined-experiment')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/e2e59/pointer-audit.json')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    result = audit(args.build_root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k not in ('findings', 'source_sha256')}, indent=2))
    return int(args.check and bool(result['fixed_site_regression_failures'] or
                                   result['classifications'].get('REQUIRES_REVIEW', 0)))


if __name__ == '__main__':
    raise SystemExit(main())
