"""Audit native entry callback ownership, original progress and SDK package."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
BASE=ROOT/'build/matrixgame/e2e568-native-services'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
    profiles={name:read(BASE/(name+'-final' if name!='sdk' else name)/'result.json') for name in ('host','arm64','sdk')}
    for name,result in profiles.items():
        assert result['status'].startswith('PASS'),name
        assert all(c['exit']==0 for c in result['commands']),name
        check_hashes(result.get('source_sha256',{}))
        if name!='sdk':
            for marker in ('copied_table=PASS original_progress_dispatch=1 Init_prefix_cycles=2',
                           'missing_callbacks_rejected_before_Init=16 takeover_preserved=PASS rollback_unbind=PASS',
                           'NO_HOST_AUDIO_TEXT_IMPLEMENTATION NO_GPU NO_SWITCH'):
                assert marker in result['stdout'],marker
            assert result['stdout'].count('original Init exception: NATIVE_SERVICES_CAPS_PENDING')==2
            assert 'callback_failure_unbind=PASS' in result['stdout']
            assert sha(BASE/(name+'-final')/'native-services')==result['binary_sha256']
            check_hashes(result['reused_production_objects_sha256'])
            for path,digest in result['archive_sha256'].items():assert sha(Path(path.replace('/mnt/d/','D:/')))==digest
    sdk=BASE/'sdk';metadata=read(sdk/'native-diagnostic-build.json');previous=read(ROOT/'build/matrixgame/e2e567-screenshot/sdk/native-diagnostic-build.json')
    check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],sdk/'source')
    assert metadata['generated_original_sha256']==previous['generated_original_sha256']
    header='port/matrixgame/native-engine/native_rangers_services.hpp'
    assert metadata['own_source_sha256'][header]==sha(ROOT/header)
    entry=(ROOT/'port/matrixgame/native-engine/original_game.cpp').read_text()
    body=entry[entry.index('void NativeInitRuntime::InitializeNativeGame('):entry.index('bool NativeInitRuntime::OriginalGameReady()')]
    assert body.index('make_unique<NativeRangersServicesScope>')<body.index('make_unique<output_paths::NativeScope>')<body.index('InitializeOriginalGame(original)')<body.index('services_scope_=std::move(services)')
    runtime=(ROOT/'port/matrixgame/native-engine/init_runtime.cpp').read_text()
    start=runtime.index('void NativeInitRuntime::Deinit() noexcept');body=runtime[start:runtime.index('void NativeInitRuntime::InitializeCursor()',start)]
    assert body.index('FinishOriginalGame()')<body.index('resource_scope_.reset()')<body.index('services_scope_.reset()')
    symbols=(sdk/'symbols.log').read_text();assert 'NativeInitRuntime::InitializeNativeGame(' in symbols
    check_hashes(profiles['sdk']['link_inputs_sha256']);assert not profiles['sdk']['strong_undefined']
    for name,value in profiles['sdk']['artifacts'].items():assert sha(ROOT/name)==value['sha256']
    for name,digest in profiles['sdk']['protected_after'].items():assert sha(Path(name))==digest
    previous_nro=ROOT/'build/matrixgame/e2e567-screenshot/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
    assert sha(previous_nro)=='10fb5e0507d8aa6b5c1ce2d4b8f50b47b5f87e1ec3816d7d29f5854efb7d882f'
    package=sdk/'diagnostic-nro'
    validation=verify_nro((package/'matrixgame-native-diagnostic.nro').read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),(package/'control.nacp').read_bytes())
    protected_report=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
    protected={name:value['sha256'] for name,value in protected_report['protected'].items() if value.get('status')!='UNAVAILABLE'}
    protected.update({name:value['sha256'] for name,value in protected_report['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
    primary=protected_report['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==protected_report['root_cumulative_NRO_verified_against_documented_hash']
    git='C:/Program Files/Git/cmd/git.exe';upstream=ROOT/'upstream/matrixgame'
    pin=subprocess.check_output([git,'-C',upstream,'rev-parse','HEAD'],text=True).strip();assert pin=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
    assert not subprocess.check_output([git,'-C',upstream,'status','--porcelain'],text=True)
    result={'status':'PASS; COMPLETE CALLBACK TABLE OWNERSHIP AND INIT PREFIX; REAL HOST SERVICES STILL REQUIRED','profiles':profiles,
        'generated_original_unchanged':True,'NRO_validation':validation,'previous_E567_NRO_preserved':sha(previous_nro),
        'protected_verified':len(protected)+2,'upstream_pin':pin,'upstream_clean':True,
        'scope':'Complete copied callback table, actual original CLoadProgress delivery and Init prefix rollback. No real sound/text/music host implementation or full native Init is claimed.',
        'remaining':['actual Rangers host resource/audio/text/music services in standalone diagnostic','successful native Init/map and teardown callbacks on device','hardware image/VRAM/HID/focus/presentation/recovery/F9'],
        'Switch':'NOT RUN','commit':False,'push':False}
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print(json.dumps(profiles['sdk']['artifacts'],indent=2))
if __name__=='__main__':main()
