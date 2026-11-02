"""Audit real Rangers music stream, ownership and explicitly limited SDK proof."""
import argparse
import json
import re
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e571-rangers-music'

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
    profiles={name:read(BASE/(name+'-final')/'result.json') for name in ('host','asan','sdk')}
    for name,result in profiles.items():
        assert result['status'].startswith('PASS'),name
        check_hashes(result['source_sha256'])
        assert all(command['exit']==0 for command in result['commands']),name
        if name=='sdk':continue
        check_hashes(result['reused_original_objects_sha256']);assert len(result['reused_original_objects_sha256'])==331
        check_hashes(result['new_objects_sha256'])
        assert sha(BASE/(name+'-final')/'rangers-music')==result['binary_sha256']
        assert result['protected_before']==result['protected_after']
        for path,digest in result['protected_after'].items():assert sha(Path(path))==digest
        assert '-UNDEBUG' in result['commands'][0]['command']
        for field,sound in (('stdout',1),('music_only_stdout',0)):
            log=result[field]
            assert f'MODE sound_enabled={sound} music_enabled=1 case_normalization=PASS playlist_membership=PASS' in log
            assert 'worker_teardown=PASS preflight=PASS duplicate=PASS' in log
            assert 'original_progress_no_main_device=PASS original_release_empty_textures=PASS' in log
            assert 'SDL_DUMMY_ONLY NO_AUDIBLE_OUTPUT NO_GUI NO_MATRIX_INIT NO_SWITCH' in log
            selected,frames,callbacks,rejected=re.search(r'REAL_MUSIC selected=(.+) played_frames=(\d+) callbacks=(\d+) ready=\d+ rejected=(\d+)',log).groups()
            assert selected.startswith('music\\') and int(frames)>=1024 and int(callbacks)>0 and int(rejected)==0
    assert profiles['asan']['new_TUs_ASan_UBSan'] and not profiles['asan']['reused_original_TUs_instrumented']
    sdk=profiles['sdk'];assert not sdk['strong_undefined'] and not sdk['new_host_code_reachable_in_probe'];check_hashes(sdk['link_inputs_sha256'])
    for path,value in sdk['artifacts'].items():assert sha(ROOT/path)==value['sha256']
    check_hashes(read(BASE/'sdk-final/music-source.json')['source_sha256'])
    assert 'RangersMusicSession::RangersMusicSession(' in (BASE/'sdk-final/archive-symbols.log').read_text()
    assert 'RangersMusicSession::RangersMusicSession(' not in (BASE/'sdk-final/symbols.log').read_text()
    provenance=read(BASE/'sdk-final/core/source/source-provenance.json');check_hashes(provenance['outputs_sha256'],BASE/'sdk-final/core/source')
    previous=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
    protected={name:value['sha256'] for name,value in previous['protected'].items() if value.get('status')!='UNAVAILABLE'}
    protected.update({name:value['sha256'] for name,value in previous['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
    primary=previous['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==previous['root_cumulative_NRO_verified_against_documented_hash']
    nro=ROOT/'build/matrixgame/e2e568-native-services/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
    assert sha(nro)=='d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750'
    git='C:/Program Files/Git/cmd/git.exe';pins={}
    for name,pin in (('matrixgame','218115a72fa9c5d43269f4ca84a8596a0bcb82f1'),('cpp','57fa689c630193a66fdea6ca4c79a188814991cd')):
        folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()
        assert pins[name]==pin and not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True)
    result={'status':'PASS; REAL RANGERS MUSIC; FULL SDK HOST LINK OPEN','profiles':profiles,'upstream_pins_clean':pins,
        'latest_E568_NRO_preserved':sha(nro),'protected_verified':len(protected)+2,'new_NRO':False,'Switch':'NOT RUN','commit':False,'push':False,
        'scope':'Both sound+music and music-only, original Music.Robot selection with real Rus CaseConv, real Vorbis workers consumed by SDL dummy. ASan/UBSan only five new TUs; 331 reused original units uninstrumented. SDK archive compile plus existing bridge-only link.',
        'remaining':['full SDK Rangers host dependency link and native runtime composition','general host configuration owner/mod/language selection integration','populated GUI progress/texture-release branches','full native CGame::Init/map and Switch graphics/input/F9/audio/hardware validation']}
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print(json.dumps(sdk['artifacts'],indent=2))
if __name__=='__main__':main()
