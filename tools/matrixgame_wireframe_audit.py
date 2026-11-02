"""Audit optional selection wireframe integration without hardware claims."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
from matrixgame_wireframe_checks import BASE


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
    output=parser.parse_args().output
    profiles={}
    for name in ('baseline','host','arm64','windows','host/stored','sdk'):
        result=read(BASE/name/'result.json');assert result['status'].startswith('PASS;'),name
        assert all(c['exit']==0 for c in result['commands']),name
        profiles[name]=result
        if name not in ('baseline','sdk'):check_hashes(result['source_sha256'])
    assert profiles['host']['stdout']==profiles['arm64']['stdout']
    assert 'BASELINE original selection WIREFRAME' in profiles['baseline']['stdout']
    assert 'cases=576 mismatches=0 different_pixels=0 max_delta=0' in profiles['windows']['stdout']
    assert 'cases=96 mismatches=0 coverage_mismatches=0 max_delta=0' in profiles['windows']['stdout']
    assert 'SELECTION_STENCIL_SEQUENCE cases=8 mismatches=0' in profiles['windows']['stdout']
    for name,digest in profiles['baseline']['source_sha256'].items():
        if name.endswith(('native_device_state.cpp','native_device_state.hpp')):assert sha(BASE/'baseline/source'/Path(name).name)==digest
    metadata=read(BASE/'sdk/native-diagnostic-build.json');check_hashes(metadata['own_source_sha256'])
    check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source')
    previous=read(ROOT/'build/matrixgame/e2e562-single-sample/sdk/native-diagnostic-build.json')
    assert metadata['generated_original_sha256']==previous['generated_original_sha256']
    sdk=profiles['sdk'];assert sdk['strong_undefined']==[]
    nro=BASE/'sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
    validation=verify_nro(nro.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),(nro.parent/'control.nacp').read_bytes())
    for name,digest in sdk['protected_after'].items():assert sha(Path(name))==digest
    preserved={}
    for stage,expected in [('e2e561-unmanaged-bitmap','c7217b96fa50bc2d2b18d595e2d0f778852ee9a2bafad20c3111fb506d9b2c4f'),('e2e562-single-sample','15ce098bd30ab5ec37d9212fb1d53d63e4f08bb43769a2fd319c1a04defc5c9a')]:
        path=ROOT/'build/matrixgame'/stage/'sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
        assert sha(path)==expected;preserved[str(path.relative_to(ROOT))]=expected
    baseline=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
    protected={name:value['sha256'] for name,value in baseline['protected'].items() if value.get('status')!='UNAVAILABLE'}
    protected.update({name:value['sha256'] for name,value in baseline['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
    primary=baseline['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==baseline['root_cumulative_NRO_verified_against_documented_hash']
    git='C:/Program Files/Git/cmd/git.exe';upstream=ROOT/'upstream/matrixgame'
    pin=subprocess.check_output([git,'-C',upstream,'rev-parse','HEAD'],text=True).strip()
    assert pin=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
    assert not subprocess.check_output([git,'-C',upstream,'status','--porcelain'],text=True)
    result={'status':'PASS; OPTIONAL ORIGINAL SELECTION WIREFRAME SOFTWARE INTEGRATION; SWITCH NOT RUN',
        'profiles':profiles,'nro_validation':validation,'preserved_NRO':preserved,'protected_verified':len(protected)+2,
        'source_changes_since_E562':[name for name,digest in previous['own_source_sha256'].items() if sha(ROOT/name)!=digest],
        'new_sources_since_E562':sorted(set(metadata['own_source_sha256'])-set(previous['own_source_sha256'])),
        'installed_deko_headers_sha256':{name:sha(Path('C:/devkitPro/libnx/include')/name) for name in ('deko3d.h','deko3d.hpp')},
        'upstream_clean':True,'upstream_pin':pin,'Switch':'NOT RUN','native_Init':'NOT RUN; last journal still stopped at ReadParams/ApplyGammaRamp before map',
        'limits':'Wireframe requires aliased lines and LASTPIXEL=TRUE. Point fill, point topology, AA lines, scissor and multisample targets remain unsupported. GPU reference is Windows HAL vs production GLSL on host GPU; no deko command execution or full original optional selection map run.',
        'commit':False,'push':False}
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print(json.dumps(sdk['artifacts'],indent=2))


if __name__=='__main__':main()
