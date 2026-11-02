"""Preserved-input audit and explicit evidence limits for the bitmap port."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT, read, sha, check_hashes
from matrixgame_diagnostic_package import verify_nro

BASE=ROOT/'build/matrixgame/e2e561-unmanaged-bitmap'


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True)
    output=parser.parse_args().output
    profiles={kind:read(BASE/kind/'result.json') for kind in ('host','arm64','sdk')}
    for kind,result in profiles.items():
        assert result['status'].startswith('PASS;'),kind
        assert all(c['exit']==0 for c in result['commands']),kind
        if kind!='sdk':
            check_hashes(result['source_sha256'])
            for name,digest in result['archive_sha256'].items():assert sha(Path(name.replace('/mnt/d/','D:/')))==digest
            assert sha(BASE/kind/'observed.bin')==result['observations_sha256']
    assert profiles['host']['observations_sha256']==profiles['arm64']['observations_sha256']
    reference=read(BASE/'expanded/windows-reference/result.json')
    assert reference['status'].startswith('PASS;') and len(reference['cases'])==156
    for case in reference['cases']:
        assert sha(ROOT/case['dds'])==case['sha256']
        assert sha(ROOT/case['oracle'])==case['oracle_sha256']
    sdk=BASE/'sdk';metadata=read(sdk/'native-diagnostic-build.json')
    check_hashes(metadata['own_source_sha256'])
    check_hashes(metadata['generated_original_sha256'],sdk/'source')
    old=read(ROOT/'build/matrixgame/e2e558-gamma/sdk/native-diagnostic-build.json')
    assert metadata['generated_original_sha256']==old['generated_original_sha256']
    nro=sdk/'diagnostic-nro/matrixgame-native-diagnostic.nro'
    validation=verify_nro(nro.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),(nro.parent/'control.nacp').read_bytes())
    for name,digest in profiles['sdk']['protected_after'].items():assert sha(Path(name))==digest
    baseline=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
    protected={name:value['sha256'] for name,value in baseline['protected'].items() if value.get('status')!='UNAVAILABLE'}
    protected.update({name:value['sha256'] for name,value in baseline['extra_frozen_artifacts_verified'].items()})
    check_hashes(protected)
    primary=baseline['primary_NRO_verified_against_previous_fingerprint']
    assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==baseline['root_cumulative_NRO_verified_against_documented_hash']
    historical={}
    for stage in ('e2e527-output','e2e529-stored-state'):
        historical[stage]={}
        for path in sorted((ROOT/'build/matrixgame'/stage).glob('*/result.json')):
            saved=read(path);assert saved['status']=='PASS' and all(c['exit']==0 for c in saved['commands'])
            historical[stage][path.parent.name]={'status':saved['status'],'result_sha256':sha(path),'rerun':False,
                'source_drift_since_historical_run':[name for name,digest in saved['source_sha256'].items() if sha(ROOT/name)!=digest]}
        assert {'host','arm64','windows','sdk'}<=historical[stage].keys()
    git='C:/Program Files/Git/cmd/git.exe'
    upstream=ROOT/'upstream/matrixgame'
    upstream_status=subprocess.check_output([git,'-C',upstream,'status','--porcelain'],text=True)
    pin=subprocess.check_output([git,'-C',upstream,'rev-parse','HEAD'],text=True).strip()
    assert not upstream_status and pin=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
    result={'status':'PASS; SOFTWARE BITMAP DEPENDENCY + SDK PACKAGE; NOT FULL NATIVE INIT',
        'profiles':profiles,'windows_reference':{'result_sha256':sha(BASE/'expanded/windows-reference/result.json'),'cases':156,'scope':reference['scope']},
        'pixel_contract':'Exact descriptors and layouts; max one channel unit versus D3DX on resampled inputs. POT copy and constants exact. CPU host and ARM64 output byte-identical.',
        'observations_sha256':profiles['host']['observations_sha256'],'nro_validation':validation,
        'native_init':'Not rerun; last hardware run stopped in ReadParams/ApplyGammaRamp, before map creation. Gamma candidate has no hardware result.',
        'new_dependency':'Original one-argument CTexture::LoadFromBitmap implemented; not currently reachable in diagnostic ELF after linker GC.',
        'historical_checks_not_rerun':historical,'protected_verified':len(protected)+2,'upstream_pin':pin,'upstream_clean':True,
        'hardware':'NOT RUN; user currently has no Switch','commit':False,'push':False}
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print('observations_sha256='+result['observations_sha256'])


if __name__=='__main__':main()
