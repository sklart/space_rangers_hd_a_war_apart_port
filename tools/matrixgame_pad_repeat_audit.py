"""Audit actual original pad-hold regression and current SDK package."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
from matrixgame_pad_repeat_checks import BASE


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
    profiles={}
    for name in ('baseline','host','arm64','input/host','input/arm64','map-baseline/windows','map-candidate/windows','sdk'):
        result=read(BASE/name/'result.json');profiles[name]=result
        if name=='map-baseline/windows':
            assert result['status']=='FAIL' and result['commands'][-1]['label']=='runtime' and result['commands'][-1]['exit']==10
            assert all(c['exit']==0 for c in result['commands'][:-1])
            assert 'PAD_HELD_UNIT_ENTER unexpected_mode_change=1 held_ms=500 frames=20' in result['stdout']
            assert 'holding gamepad X repeated original robot-mode toggle' in result['stdout']
        else:
            assert result['status'].startswith('PASS'),name
            assert all(c['exit']==0 for c in result['commands']),name
        if name not in ('baseline','sdk'):check_hashes(result['source_sha256'])
    assert profiles['host']['stdout']==profiles['arm64']['stdout']
    for name,digest in profiles['baseline']['source_sha256'].items():assert sha(BASE/'baseline/source'/Path(name).name)==digest
    for kind in ('host','arm64'):
        assert 'mouse_key_aliases=3 release_orders=checked' in profiles['input/'+kind]['stdout']
    candidate=profiles['map-candidate/windows']
    for text in ('PAD_HELD_UNIT_ENTER held_ms=800 frames=32 unexpected_mode_change=0 original_robot_retained=1',
                 'REAL_SELECTION mouse_drag=PASS','mouse_held_after_pad_key_release=1 fire_order_after_pad_key_release=1',
                 'REAL_FORM_LOOP draws=8 elapsed=125','PrepareMap complete'):
        assert candidate['stdout'].count(text)==2,text
    assert 'ACTUAL_MAP_HAL_RESULT=0 NATIVE_STATE_REJECTED_SESSIONS=0' in candidate['stdout']
    baseline=profiles['map-baseline/windows']
    assert sha(BASE/'map-baseline/windows/matrixgame-map-state.exe')==baseline['binary_sha256']
    assert sha(BASE/'map-candidate/windows/matrixgame-map-state.exe')==candidate['binary_sha256']
    old_cpp=BASE/'baseline/source/native_input.cpp'
    assert any(c['label']=='build-native_input' for c in baseline['commands'])
    command=(BASE/'map-baseline/windows/build-native_input.cmd').read_text()
    assert str(old_cpp) in command
    for name,digest in candidate.get('capture_sha256',{}).items():assert sha(ROOT/name)==digest
    sdk=BASE/'sdk';metadata=read(sdk/'native-diagnostic-build.json');check_hashes(metadata['own_source_sha256'])
    check_hashes(metadata['generated_original_sha256'],sdk/'source')
    previous=read(ROOT/'build/matrixgame/e2e563-wireframe/sdk/native-diagnostic-build.json')
    assert metadata['generated_original_sha256']==previous['generated_original_sha256']
    package=sdk/'diagnostic-nro';nro=package/'matrixgame-native-diagnostic.nro'
    validation=verify_nro(nro.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),(package/'control.nacp').read_bytes())
    assert profiles['sdk']['strong_undefined']==[]
    for name,digest in profiles['sdk']['protected_after'].items():assert sha(Path(name))==digest
    previous_nro=ROOT/'build/matrixgame/e2e563-wireframe/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
    assert sha(previous_nro)=='d88b01d85c0c977fcbdfa68bdf9dbe15343f7b886dd9390ea1122583035d5364'
    protected_report=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
    protected={name:value['sha256'] for name,value in protected_report['protected'].items() if value.get('status')!='UNAVAILABLE'}
    protected.update({name:value['sha256'] for name,value in protected_report['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
    primary=protected_report['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==protected_report['root_cumulative_NRO_verified_against_documented_hash']
    git='C:/Program Files/Git/cmd/git.exe';upstream=ROOT/'upstream/matrixgame'
    pin=subprocess.check_output([git,'-C',upstream,'rev-parse','HEAD'],text=True).strip();assert pin=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
    assert not subprocess.check_output([git,'-C',upstream,'status','--porcelain'],text=True)
    sound_names=['port/matrixgame/native/engine_callbacks.cpp','port/matrixgame/native/rangers_bridge.cpp',
        'port/matrixgame/native-engine/original_game.cpp','port/switch/platform/e2e4_game_audio_bridge.cpp',
        'upstream/matrixgame/MatrixGame/src/MatrixSoundManager.cpp']
    result={'status':'PASS; ORIGINAL PAD-HOLD ROBOT REGRESSION FIX; NATIVE HID HARDWARE NOT RUN','profiles':profiles,
        'nro_validation':validation,'upstream_clean':True,'upstream_pin':pin,'protected_verified':len(protected)+2,
        'previous_E563_NRO_preserved':sha(previous_nro),'production_change':'Keyboard-only typematic; merged held keys and mouse ownership preserved.',
        'source_changes_since_E563':[name for name,digest in previous['own_source_sha256'].items() if sha(ROOT/name)!=digest],
        'sound_boundary_audit':{'source_sha256':{name:sha(ROOT/name) for name in sound_names},
            'conclusion':'Original CSound routes symbolic Sound.* names through Rangers host callbacks. Existing native bridge has a full callback contract; standalone diagnostic has not bound real Rangers services. SDL PCM output alone is insufficient. No sound playback or integration is claimed.'},
        'Switch':'NOT RUN','native_Init':'NOT RUN; last Switch journal remains ReadParams/ApplyGammaRamp before map',
        'commit':False,'push':False}
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print(json.dumps(profiles['sdk']['artifacts'],indent=2))


if __name__=='__main__':main()
