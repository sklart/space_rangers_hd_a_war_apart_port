"""Compile/link the actual navigation/resource CPU chain with devkitA64; never execute it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from matrixgame_engine_prepare import prepare_engine

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, default=Path('C:/devkitPro') if os.name == 'nt' else Path('/opt/devkitpro'))
    parser.add_argument('--output', type=Path, default=ROOT/'build/e2e515/mg-navigation-sdk')
    parser.add_argument('--contract',choices=('navigation','lifecycle','big-buffers'),default='navigation')
    args = parser.parse_args()
    out = args.output.resolve()
    if not out.is_relative_to(ROOT/'build') or (out.exists() and any(out.iterdir())):
        raise ValueError('Use a fresh isolated build output')
    out.mkdir(parents=True,exist_ok=True)
    sdk = args.sdk.resolve()
    suffix = '.exe' if os.name == 'nt' else ''
    compiler = sdk/('devkitA64/bin/aarch64-none-elf-g++'+suffix)
    readelf = sdk/('devkitA64/bin/aarch64-none-elf-readelf'+suffix)
    report = dict(status='RUNNING', scope='devkitA64 compile/link only', runtime_executed=False,
                  battle_executed=False, hardware_executed=False)
    def run(command, log):
        result = subprocess.run(list(map(str, command)), cwd=ROOT, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True,
                                env={**os.environ, 'DEVKITPRO':sdk.as_posix(),
                                     'DEVKITA64':(sdk/'devkitA64').as_posix()})
        (out/log).write_text(result.stdout)
        if result.returncode: raise RuntimeError(f'{log}: exit {result.returncode}')
        return result.stdout
    try:
        provenance = prepare_engine(out/'source')
        report['source_commit'] = provenance['source_commit']
        core = out/'source/portable-core'
        test={'navigation':'test_matrixgame_navigation.cpp','lifecycle':'test_matrixgame_native_lifecycle.cpp','big-buffers':'test_matrixgame_big_buffers.cpp'}[args.contract]
        sources = [ROOT/'tests'/test,
                   out/'source/MatrixGame/src/Logic/MatrixRoadNetwork.cpp']
        sources += [core/name for name in ('CBuf.cpp','CBuf_file.cpp','CStorage.cpp','CRC32.cpp','CMain.cpp','Form.cpp','CReminder.cpp')]
        sources += [ROOT/'port/matrixgame/native-core'/name for name in
                    ('cbuf_utf16.cpp','exception_native.cpp','file_native.cpp','heap_native.cpp')]
        sources += [ROOT/'port/switch/platform/package.cpp']
        if args.contract=='big-buffers':
            sources += [ROOT/'port/matrixgame/native-core'/name for name in ('buffer_resources.cpp','buffer_geometry.cpp','vector_geometry.cpp')]
        elf = out/(args.contract+'-contract.elf')
        report['contract']=args.contract
        command = [compiler,'-std=c++20','-O2','-g','-UNDEBUG','-D_RELDEBUG','-D__SWITCH__',
                   '-D_POSIX_C_SOURCE=200809L','-march=armv8-a+crc+crypto','-mtune=cortex-a57','-mtp=soft','-fPIE',
                   '-ffunction-sections','-fdata-sections','-ffp-contract=off']
        for include in (core,out/'source/MatrixGame/src/Logic',ROOT/'port/matrixgame/native-core',
                        ROOT/'port/switch/platform',sdk/'portlibs/switch/include',sdk/'libnx/include',
                        ROOT/'port/matrixgame/third_party/directxmath/Inc',ROOT/'port/matrixgame/third_party/sal'):
            command += ['-I'+include.as_posix()]
        command += sources + ['-L'+(sdk/'portlibs/switch/lib').as_posix(),
                              '-L'+(sdk/'libnx/lib').as_posix(),'-lz','-lnx',
                              '-specs='+(sdk/'libnx/switch.specs').as_posix(),
                              '-Wl,--gc-sections','-Wl,--build-id=sha1','-o',elf]
        report['command'] = list(map(str, command))
        run(command,'compile-link.log')
        header = run([readelf,'-h',elf],'elf-header.log')
        if 'AArch64' not in header or 'ELF64' not in header: raise RuntimeError('Wrong ELF architecture')
        symbols = run([readelf,'-sW',elf],'elf-symbols.log')
        undefined = [line for line in symbols.splitlines() if ' UND ' in line and ' WEAK ' not in line and len(line.split())>7]
        if undefined: raise RuntimeError('Unresolved strong symbols: '+str(undefined))
        required_symbols={'navigation':('FindPathInRegionRun','CMatrixRoadNetwork4Load','CMatrixRoadNetwork4Save','BaseInit'),
                          'lifecycle':('FormChange','BaseInit','CForm10StaticInit'),
                          'big-buffers':('CaptureBoundVO','ValidateBoundVO','BindVertex','CreateVertex')}[args.contract]
        for required in required_symbols:
            if required not in symbols: raise RuntimeError('Missing real implementation: '+required)
        notes = run([readelf,'-n',elf],'elf-notes.log')
        report.update(status='PASS', sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
                      bytes=elf.stat().st_size, undefined_strong=undefined,
                      build_id=next(line.strip() for line in notes.splitlines() if 'Build ID:' in line),
                      inputs_sha256={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sources})
    except Exception as error:
        report.update(status='FAIL', error=str(error))
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('command','inputs_sha256')},indent=2))
    return 0 if report['status']=='PASS' else 1

if __name__ == '__main__': raise SystemExit(main())
