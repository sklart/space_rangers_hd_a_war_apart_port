"""Audit the single-sample fix, raw original captures and preserved packages."""
import argparse
import json
import struct
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
from matrixgame_single_sample_checks import BASE


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
    output=parser.parse_args().output
    profiles={}
    for name in ('baseline','host','arm64','windows','host/stored','arm64/stored','windows/stored','host/replay','windows-reference','sdk'):
        result=read(BASE/name/'result.json');assert result['status'].startswith('PASS;'),name
        assert all(c['exit']==0 for c in result['commands']),name
        profiles[name]=result
        if name not in ('baseline','sdk'):check_hashes(result['source_sha256'])
        for capture,digest in result.get('captures_sha256',{}).items():assert sha(ROOT/capture)==digest
    assert profiles['host']['stdout']==profiles['arm64']['stdout']==profiles['windows']['stdout']
    assert profiles['host/stored']['stdout']==profiles['arm64/stored']['stdout']==profiles['windows/stored']['stdout']
    assert 'BASELINE original UI restore' in profiles['baseline']['stdout']
    assert 'all_Draw_readback_bytes_equal=PASS' in profiles['windows-reference']['stdout']
    pixels=BASE/'windows-reference/pixels.bgra'
    assert pixels.stat().st_size==8*64*64*4 and sha(pixels)==profiles['windows-reference']['pixels_sha256']
    original=read(ROOT/'build/matrixgame/e2e543-journal/map-state/windows/result.json')
    captures=[]
    for name,digest in original['capture_sha256'].items():
        path=ROOT/name;assert sha(path)==digest;data=path.read_bytes()
        magic,version,size,count=struct.unpack_from('<4I',data)
        assert magic==0x3647524d and version==1 and size==2808 and len(data)==16+count*(size+8)
        total=on=masked=multi=0
        for i in range(count):
            at=16+i*(size+8);samples=struct.unpack_from('<I',data,at+24)[0]
            enable,mask=struct.unpack_from('<2I',data,at+32+59*4)
            occurrences=struct.unpack_from('<Q',data,at+size)[0]
            total+=occurrences;on+=occurrences*bool(enable);masked+=occurrences*(mask!=0xffffffff);multi+=occurrences*bool(samples)
        assert not multi
        captures.append({'path':name,'sha256':digest,'unique':count,'draws':total,'raw_MSAA_TRUE_draws':on,'nondefault_mask_draws':masked,'multisample_target_draws':multi})
    assert sum(c['raw_MSAA_TRUE_draws'] for c in captures)>0
    adapter=(ROOT/'tests/matrixgame_actual_draw_state.hpp').read_text()
    assert 'if(id==161' not in adapter and 'n.Render(id,s.render[i])' in adapter
    metadata=read(BASE/'sdk/native-diagnostic-build.json');check_hashes(metadata['own_source_sha256'])
    check_hashes(metadata['generated_original_sha256'],BASE/'sdk/source')
    prior=read(ROOT/'build/matrixgame/e2e561-unmanaged-bitmap/sdk/native-diagnostic-build.json')
    assert prior['generated_original_sha256']==metadata['generated_original_sha256']
    sdk=profiles['sdk'];assert sdk['strong_undefined']==[]
    nro=BASE/'sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
    validation=verify_nro(nro.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),(nro.parent/'control.nacp').read_bytes())
    for name,digest in sdk['protected_after'].items():assert sha(Path(name))==digest
    previous_nro=ROOT/'build/matrixgame/e2e561-unmanaged-bitmap/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
    assert sha(previous_nro)=='c7217b96fa50bc2d2b18d595e2d0f778852ee9a2bafad20c3111fb506d9b2c4f'
    baseline=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
    protected={name:value['sha256'] for name,value in baseline['protected'].items() if value.get('status')!='UNAVAILABLE'}
    protected.update({name:value['sha256'] for name,value in baseline['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
    primary=baseline['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==baseline['root_cumulative_NRO_verified_against_documented_hash']
    git='C:/Program Files/Git/cmd/git.exe';upstream=ROOT/'upstream/matrixgame'
    pin=subprocess.check_output([git,'-C',upstream,'rev-parse','HEAD'],text=True).strip()
    assert pin=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
    assert not subprocess.check_output([git,'-C',upstream,'status','--porcelain'],text=True)
    result={'status':'PASS; SINGLE-SAMPLE STATE FIX; NATIVE INIT NOT RUN','profiles':profiles,'raw_capture_counts':captures,
        'nro_validation':validation,'previous_E561_NRO_preserved':sha(previous_nro),'protected_verified':len(protected)+2,
        'source_changes_since_E561':[name for name,digest in prior['own_source_sha256'].items() if sha(ROOT/name)!=digest],
        'upstream_clean':True,'upstream_pin':pin,'Switch':'NOT RUN','commit':False,'push':False,
        'limitations':'CPU replay validates state acceptance and preparation, not geometry/images on deko. HAL pixels prove D3D9 single-sample semantics only. Actual MSAA targets and resolve remain unsupported.'}
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print(json.dumps(captures,indent=2))


if __name__=='__main__':main()
