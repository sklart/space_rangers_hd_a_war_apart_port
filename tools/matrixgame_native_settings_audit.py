"""Audit preserved settings, original consumers, native AF and SDK evidence."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro

BASE=ROOT/'build/matrixgame/e2e565-native-settings'

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
    profiles={}
    names=('host','arm64','windows','config-host','config-arm64','config-windows','entry-host','entry-arm64','aniso-baseline','aniso-host','aniso-arm64','sdk')
    for name in names:
        result=read(BASE/name/'result.json');profiles[name]=result
        assert result['status'].startswith('PASS'),name
        assert all(c['exit']==0 for c in result['commands']),name
        check_hashes(result.get('source_sha256',{}))
    observations={}
    for prefix,file in (('','settings.bin'),('config-','observed.bin')):
        values={kind:sha(BASE/(prefix+kind)/file) for kind in ('host','arm64','windows')}
        assert len(set(values.values()))==1,values
        for kind,digest in values.items():assert profiles[prefix+kind]['observations_sha256']==digest
        observations[prefix or 'settings']=values
    for name in ('entry-host','entry-arm64'):
        log=profiles[name]['stdout']
        assert 'provided_entry=1 rejected_before_Init=7 caller_unchanged=PASS' in log
        assert 'two original cleanup cycles/output unbind PASS' in log
        assert log.count('original Init exception: NATIVE_MATERIAL_CAPS_PENDING')==2
    assert 'REPRODUCED' in profiles['aniso-baseline']['stdout']
    for name in ('aniso-host','aniso-arm64'):
        assert 'cases=160 actual_SDK_samplers=160' in profiles[name]['stdout']
        assert 'raw_invalid_filters_rejected=PASS error_propagation=PASS' in profiles[name]['stdout']
    sdk=BASE/'sdk';metadata=read(sdk/'native-diagnostic-build.json')
    check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],sdk/'source')
    previous=read(ROOT/'build/matrixgame/e2e564-pad-repeat/sdk/native-diagnostic-build.json')
    old=previous['generated_original_sha256'];new=metadata['generated_original_sha256']
    assert old.keys()==new.keys()
    changes=[name for name in old if old[name]!=new[name]]
    assert changes==['MatrixGame/src/MatrixSampleStateManager.cpp'],changes
    sampler='MatrixGame/src/MatrixSampleStateManager.cpp'
    assert sha(BASE/'source'/sampler)==new[sampler]
    ninja=(sdk/'build.ninja').read_text()
    assert 'build CMakeFiles/matrixgame-native-cache.dir/source/MatrixGame/src/MatrixSampleStateManager.cpp.obj: CXX_' in ninja
    policy=(sdk/'source'/sampler).read_text()
    assert '#ifdef _WIN32' in policy and 'ANISOTROPIC is undefined for MIPFILTER' in policy
    assert 'return type == srhd_awa::matrixgame::SamplerRegister::MinFilter;' in policy
    check_hashes(profiles['sdk']['link_inputs_sha256'])
    for name,value in profiles['sdk']['artifacts'].items():assert sha(ROOT/name)==value['sha256']
    assert profiles['sdk']['strong_undefined']==[]
    for name,digest in profiles['sdk']['protected_after'].items():assert sha(Path(name))==digest
    previous_nro=ROOT/'build/matrixgame/e2e564-pad-repeat/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
    assert sha(previous_nro)=='d34b1801af5f8e66c51df5540874d155ed2c30bc7c628fdf65bf8a03822724d8'
    package=sdk/'diagnostic-nro'
    validation=verify_nro((package/'matrixgame-native-diagnostic.nro').read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),(package/'control.nacp').read_bytes())
    protected_report=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
    protected={name:value['sha256'] for name,value in protected_report['protected'].items() if value.get('status')!='UNAVAILABLE'}
    protected.update({name:value['sha256'] for name,value in protected_report['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
    primary=protected_report['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==protected_report['root_cumulative_NRO_verified_against_documented_hash']
    git='C:/Program Files/Git/cmd/git.exe';upstream=ROOT/'upstream/matrixgame'
    pin=subprocess.check_output([git,'-C',upstream,'rev-parse','HEAD'],text=True).strip()
    assert pin=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
    assert not subprocess.check_output([git,'-C',upstream,'status','--porcelain'],text=True)
    result={'status':'PASS; COPIED NATIVE SETTINGS AND MINIFICATION AF; HARDWARE NOT RUN','profiles':profiles,
        'cross_ABI_observations':observations,'generated_changes_since_E564':changes,'NRO_validation':validation,
        'upstream_pin':pin,'upstream_clean':True,'protected_verified':len(protected)+2,'previous_E564_NRO_preserved':sha(previous_nro),
        'native_Init':'CPU prefix to deliberate ReadCaps refusal only; last Switch journal remains ReadParams/ApplyGammaRamp before map',
        'remaining':['Switch gamma/presentation and subsequent original Init/map execution','actual Rangers resource/audio callback services','native map/image/VRAM/HID/focus and recovery validation'],
        'Switch':'NOT RUN','commit':False,'push':False}
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print(json.dumps(profiles['sdk']['artifacts'],indent=2))

if __name__=='__main__':main()
