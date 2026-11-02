"""Audit actual AFT null-clip regression, pixel differential and SDK scope."""
import argparse
import json
import re
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e570-rangers-text'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
    profiles={name:read(BASE/name/'result.json') for name in ('baseline','host','asan','sdk')}
    for name,result in profiles.items():
        assert result['status'].startswith('PASS'),name
        check_hashes(result['source_sha256'])
        for command in result['commands']:
            expected=11 if name=='baseline' and command['label']=='runtime' else 0
            assert command['exit']==expected,(name,command)
        if name!='sdk':
            check_hashes(result['reused_original_objects_sha256']);assert len(result['reused_original_objects_sha256'])==331
            check_hashes(result['new_objects_sha256']);check_hashes(result['output_sha256'])
            assert sha(BASE/name/'rangers-text')==result['binary_sha256']
            assert result['protected_before']==result['protected_after']
            for path,digest in result['protected_after'].items():assert sha(Path(path))==digest
            assert 'original_AFT_explicit_clip=PASS' in result['stdout']
            assert '-UNDEBUG' in result['commands'][0]['command']
    for name in ('host','asan'):
        assert 'measured=156x16' in profiles[name]['stdout']
        assert 'exact_original_pairs=24 crop=1 automatic=1 error_cleanup=PASS' in profiles[name]['stdout']
        assert 'CPU_AFT_RASTER_ONLY NO_GUI NO_MATRIX_INIT NO_SWITCH' in profiles[name]['stdout']
    assert profiles['asan']['new_TUs_ASan_UBSan'] and not profiles['asan']['reused_original_TUs_instrumented']
    host_pixels=BASE/'host/runtime/pixels.bin';asan_pixels=BASE/'asan/runtime/pixels.bin'
    assert host_pixels.stat().st_size==24*200*64*4 and host_pixels.read_bytes()==asan_pixels.read_bytes()
    backtrace=BASE/'baseline/null-clip-backtrace.log';trace=backtrace.read_text(encoding='utf-8')
    assert 'SIGSEGV' in trace and 'SourceClip = pas::load_unaligned<Types::TRect>(Clip)' in trace and 'RangersBridge::RenderText' in trace
    previous=read(ROOT/'build/matrixgame/e2e569-rangers-sound/sdk/result.json')
    assert sha(BASE/'producer-baseline.cpp')==previous['source_sha256']['port/matrixgame/native/rangers_host_callbacks.cpp']
    original=(ROOT/'upstream/cpp/src/Robot.cpp').read_text();block=original[original.index('    void InitializeRobotRuntime()'):original.index('    void FinalizeRobotRuntime()')]
    mapping={field:(kind,function) for field,kind,function in re.findall(r'RobotCallbacks\.(\w+)\s*=\s*(TRobot\w+)\((Robot\w+)\);',block)}
    producer=(ROOT/'port/matrixgame/native/rangers_host_callbacks.cpp').read_text()
    current={field:(kind,function) for field,kind,function in re.findall(r'callbacks\.(\w+)\s*=\s*Robot::(TRobot\w+)\(Robot::(Robot\w+)\);',producer)}
    assert len(mapping)==16 and len(current)==15;expected=dict(mapping);del expected['RenderText'];assert current==expected
    assert 'callbacks.RenderText=Robot::TRobotRenderText(RenderHostText);' in producer
    assert 'Types::TRect full{0,0,width,height};' in producer
    assert 'Robot::RobotRenderText(text,font,color,width,height,align_x,align_y,wrap,offset_x,offset_y,clip?clip:&full,image);' in producer
    sdk=profiles['sdk'];assert not sdk['strong_undefined'] and not sdk['new_host_code_reachable_in_probe'];check_hashes(sdk['link_inputs_sha256'])
    for name,value in sdk['artifacts'].items():assert sha(ROOT/name)==value['sha256']
    assert 'RenderHostText(' in (BASE/'sdk/archive-symbols.log').read_text()
    provenance=read(BASE/'sdk/core/source/source-provenance.json');check_hashes(provenance['outputs_sha256'],BASE/'sdk/core/source')
    protected_report=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
    protected={name:value['sha256'] for name,value in protected_report['protected'].items() if value.get('status')!='UNAVAILABLE'}
    protected.update({name:value['sha256'] for name,value in protected_report['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
    primary=protected_report['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==protected_report['root_cumulative_NRO_verified_against_documented_hash']
    latest=ROOT/'build/matrixgame/e2e568-native-services/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';assert sha(latest)=='d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750'
    git='C:/Program Files/Git/cmd/git.exe';pins={}
    for name in ('matrixgame','cpp'):
        folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()
        assert not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True),name
    assert pins['matrixgame']==provenance['upstream_pin']=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
    result={'status':'PASS; REAL ORIGINAL AFT TEXT NULL-CLIP ADAPTER; FULL SDK HOST LINK OPEN','profiles':profiles,
        'pixel_observations_sha256':sha(host_pixels),'pixel_bytes':host_pixels.stat().st_size,'baseline_backtrace_sha256':sha(backtrace),
        'upstream_pins_clean':pins,'protected_verified':len(protected)+2,'latest_E568_NRO_preserved':sha(latest),'new_NRO':False,
        'scope':'24 original AFT direct-reference pairs varying Wrap/align flags with one real font and string; no independent Windows/GPU renderer comparison. ASan/UBSan only new four TUs; 331 reused original units not instrumented.',
        'remaining':['other fonts, tagged/multiline layouts and full UI coverage','real music/progress/render-release context validation','full SDK host link and native session wiring','native Init/map and Switch image/VRAM/HID/focus/presentation/recovery/F9/audio'],
        'Switch':'NOT RUN','commit':False,'push':False}
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print(json.dumps(sdk['artifacts'],indent=2))
if __name__=='__main__':main()
