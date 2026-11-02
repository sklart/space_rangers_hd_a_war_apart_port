"""Read-only audit of abort callback ownership and the separate SDK package."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
BASE=ROOT/'build/matrixgame/e2e566-readback-abort'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
    profiles={name:read(BASE/name/'result.json') for name in ('host','arm64','sdk')}
    for name,result in profiles.items():
        assert result['status'].startswith('PASS'),name
        assert all(c['exit']==0 for c in result['commands']),name
        check_hashes(result.get('source_sha256',{}))
    for name in ('host','arm64'):
        result=profiles[name]
        assert 'cases=12 retained_primary=6 late_deliveries=6' in result['baseline_stdout']
        assert 'cases=12 retained_primary=0 late_deliveries=0' in result['candidate_stdout']
        for mode in ('baseline','candidate'):
            assert sha(BASE/name/mode/'readback-abort')==result[mode+'_binary_sha256']
            source=BASE/'platform_switch-baseline.cpp' if mode=='baseline' else ROOT/'port/matrixgame/native-engine/platform_switch.cpp'
            text=source.read_text();start=text.index('\n',text.index('DeviceResult DekoDevice::AbortNativeFrame()noexcept{'))+1;end=text.index('    try{',start)
            assert (BASE/name/mode/'abort-readback-prefix.inc').read_text()==text[start:end]
    sdk=BASE/'sdk';metadata=read(sdk/'native-diagnostic-build.json')
    previous=read(ROOT/'build/matrixgame/e2e565-native-settings/sdk/native-diagnostic-build.json')
    assert sha(BASE/'platform_switch-baseline.cpp')==previous['own_source_sha256']['port/matrixgame/native-engine/platform_switch.cpp']
    check_hashes(metadata['own_source_sha256']);check_hashes(metadata['generated_original_sha256'],sdk/'source')
    assert metadata['generated_original_sha256']==previous['generated_original_sha256']
    changed=[name for name,digest in previous['own_source_sha256'].items() if sha(ROOT/name)!=digest]
    assert changed==['port/matrixgame/native-engine/platform_switch.cpp'],changed
    check_hashes(profiles['sdk']['link_inputs_sha256'])
    for name,value in profiles['sdk']['artifacts'].items():assert sha(ROOT/name)==value['sha256']
    assert profiles['sdk']['strong_undefined']==[]
    for name,digest in profiles['sdk']['protected_after'].items():assert sha(Path(name))==digest
    previous_nro=ROOT/'build/matrixgame/e2e565-native-settings/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
    assert sha(previous_nro)=='d0d760b08039d0071f49ab10244a1bdd362506dfa44b1eb0fb56364a7d8b2bd3'
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
    result={'status':'PASS; CANCEL BOTH READBACK CALLBACKS BEFORE ABORT GUARDS; HARDWARE NOT RUN','profiles':profiles,
        'production_changes_since_E565':changed,'generated_original_unchanged':True,'NRO_validation':validation,
        'previous_E565_NRO_preserved':sha(previous_nro),'protected_verified':len(protected)+2,'upstream_pin':pin,'upstream_clean':True,
        'scope':'Actual production AbortNativeFrame prefix callback retention and cancellation; CPU ownership only. No actual GPU stale callback or UAF is claimed.',
        'remaining':['F9 screenshot provider and PNG output still unimplemented','actual Rangers resource/audio services','native Init/map and device presentation/recovery validation'],
        'Switch':'NOT RUN','commit':False,'push':False}
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print(json.dumps(profiles['sdk']['artifacts'],indent=2))
if __name__=='__main__':main()
