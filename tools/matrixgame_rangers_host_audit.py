"""Verify full original-services SDK link and matching real host adapter runs."""
import argparse
import json
import re
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e572-rangers-host-sdk'

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
    sdk=read(BASE/'result.json');assert sdk['status'].startswith('PASS');check_hashes(sdk['source_sha256'])
    assert all(command['exit']==0 for command in sdk['commands'])
    for key in ('dependency_sha256','old_objects_sha256','archive_objects_sha256','native_sdk_link_inputs_sha256'):check_hashes(sdk[key])
    assert len(sdk['archive_objects_sha256'])==378 and len(sdk['stale_objects'])==58
    assert sdk['protected_before']==sdk['protected_after']
    for path,digest in sdk['protected_after'].items():assert sha(Path(path))==digest
    for path,value in sdk['artifacts'].items():assert sha(ROOT/path)==value['sha256']
    symbol_log=ROOT/next(command['log'] for command in sdk['commands'] if command['label']=='symbols')
    symbols=symbol_log.read_text()
    assert not sdk['strong_undefined'] and not any(' U ' in line for line in symbols.splitlines())
    assert all(name in symbols for name in sdk['required_linked_symbols'])
    assert 'AArch64' in sdk['elf_header'] and 'DYN (Position-Independent Executable file)' in sdk['elf_header']
    assert 'CGame::Init(' not in symbols
    linkmap=(BASE/'sdk/rangers-host-link.map').read_text()
    assert 'liboriginal-rangers-host-shared-package.a(program.o)' not in linkmap
    assert 'liboriginal-rangers-host-shared-package.a(package.o)' not in linkmap
    assert 'core/libmatrixgame-native-core.a(package.cpp.obj)' in linkmap
    original=(ROOT/'upstream/cpp/src/Robot.cpp').read_text();start=original.index('    void InitializeRobotRuntime()');stop=original.index('    void FinalizeRobotRuntime()')
    callbacks=re.findall(r'RobotCallbacks\.(\w+)\s*=\s*TRobot\w+\((Robot\w+)\);',original[start:stop]);assert len(callbacks)==16
    assert all('Robot::'+function+'(' in symbols for _,function in callbacks)
    probe=(ROOT/'tests/probe_matrixgame_rangers_host_link.cpp').read_text()
    fields=set(re.findall(r'table->(m_\w+)\(',probe));assert len(fields)==16
    profiles={name:read(BASE/name/'result.json') for name in ('host','asan')}
    for name,result in profiles.items():
        assert result['status'].startswith('PASS') and all(command['exit']==0 for command in result['commands'])
        for key in ('source_sha256','generated_headers_sha256','reused_objects_sha256','new_objects_sha256','protected_verified'):check_hashes(result[key])
        assert len(result['reused_objects_sha256'])==335
        assert sha(BASE/name/'rangers-host')==result['binary_sha256']
        assert 'REAL_HOST_ENGINE full16=PASS text=PASS sound=PASS music_stream=PASS teardown=PASS NO_MATRIX_INIT NO_SWITCH_PROOF' in result['stdout']
        assert '-UNDEBUG' in result['commands'][0]['command']
    assert profiles['asan']['new_TUs_instrumented'] and not profiles['asan']['original_dependencies_instrumented']
    assert '-fsanitize=address,undefined' in profiles['asan']['commands'][0]['command']
    previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
    protected={name:value['sha256'] for name,value in previous['protected'].items() if value.get('status')!='UNAVAILABLE'}
    protected.update({name:value['sha256'] for name,value in previous['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
    primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
    git='C:/Program Files/Git/cmd/git.exe';pins={}
    for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd'),('okgf','c01aa7a168a6f1772541501074b7bba1b96550ef')):
        folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()
        assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
    result={'status':'PASS; FULL REAL RANGERS SERVICES + ENGINE ADAPTER SDK LINK; HOST EXECUTED','sdk':sdk,'host_profiles':profiles,
        'original_callback_functions_linked':len(callbacks),'engine_callback_fields_executed':sorted(fields),'upstream_pins_clean':pins,
        'scope':'Full16 MatrixGame table to actual Rangers host sound/AFT/music path on host SDL dummy; SDK AArch64 PIE link with live producer/owners and zero strong undefined. Original reused host TUs not sanitizer instrumented. SDK ELF NOT RUN; no native CGame::Init, GPU/deko, battle or hardware proof.',
        'remaining':['general configuration/package/language/mod owner','native runtime composition and complete original Init/map','updated deko3d diagnostic NRO after composition','ARM64/QEMU and Windows execution of full host-services probe','user Switch image/audio/input/focus/recovery validation'],
        'protected_verified':len(protected)+2,'new_NRO':False,'Switch':'NOT RUN','commit':False,'push':False,
        'audit_sources_sha256':{str(path.relative_to(ROOT)):sha(path) for path in (Path(__file__).resolve(),ROOT/'tools/matrixgame_rangers_host_checks.py')}}
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print(json.dumps(sdk['artifacts'],indent=2))
if __name__=='__main__':main()
