"""Verify real Sound.* resource/PCM/SDL evidence without rerunning the game."""
import argparse
import hashlib
import io
import json
import re
import subprocess
import wave
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e569-rangers-sound'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True);output=parser.parse_args().output
    host=read(BASE/'host/result.json');asan=read(BASE/'host-asan/result.json');sdk=read(BASE/'sdk/result.json')
    for result in (host,asan,sdk):
        assert result['status'].startswith('PASS'),result['status']
        assert all(command['exit']==0 for command in result['commands'])
        check_hashes(result['source_sha256'])
    check_hashes(host['new_objects_sha256']);check_hashes(host['reused_original_objects_sha256']);check_hashes(host['original_compiled_sources_sha256'])
    assert len(host['reused_original_objects_sha256'])==329
    assert set(host['refreshed_original_sources'])=={'port/switch/platform/e2e4_quicksave_transaction.cpp','port/switch/platform/e2e515_perf.cpp'}
    check_hashes(host['output_sha256']);assert sha(BASE/'host/rangers-sound')==host['binary_sha256']
    for marker in ('bytes=6874 channels=1 rate=44100 bits=16','original_Robot_callbacks=9','original_CacheData_WAV_PCM=PASS effect_completion=PASS loop_volume_zero_stop=PASS host_cleanup=PASS','SDL_DUMMY_ONLY NO_AUDIBLE_OUTPUT NO_MATRIX_INIT NO_SWITCH'):
        assert marker in host['stdout'],marker
    assert 'backend_open_failure=REAL globals_rollback=PASS flags_restored=PASS NO_FAKE_SUCCESS' in host['backend_failure_stdout']
    for path,digest in host['protected_after'].items():assert sha(Path(path))==digest
    assert host['protected_before']==host['protected_after']
    assert asan['new_TUs_ASan_UBSan'] and not asan['reused_original_TUs_instrumented']
    check_hashes(asan['new_objects_sha256']);check_hashes(asan['reused_original_objects_sha256']);check_hashes(asan['output_sha256'])
    assert sha(BASE/'host-asan/rangers-sound')==asan['binary_sha256']
    assert 'original_Robot_callbacks=9' in asan['stdout'] and 'host_cleanup=PASS' in asan['stdout']
    assert 'backend_open_failure=REAL globals_rollback=PASS flags_restored=PASS NO_FAKE_SUCCESS' in asan['backend_failure_stdout']
    assert asan['protected_before']==asan['protected_after']
    for path,digest in asan['protected_after'].items():assert sha(Path(path))==digest
    raw=(BASE/'host/runtime/original.wav').read_bytes();pcm=(BASE/'host/runtime/original.pcm').read_bytes()
    with wave.open(io.BytesIO(raw),'rb') as decoded:
        params=decoded.getparams();assert (params.nchannels,params.sampwidth,params.framerate,params.nframes,params.comptype)==(1,2,44100,3437,'NONE')
        assert decoded.readframes(params.nframes)==pcm and len(pcm)==6874
    assert (BASE/'host-asan/runtime/original.pcm').read_bytes()==pcm
    assert (BASE/'host-asan/runtime/original.wav').read_bytes()==raw
    original=(ROOT/'upstream/cpp/src/Robot.cpp').read_text()
    block=original[original.index('    void InitializeRobotRuntime()'):original.index('    void FinalizeRobotRuntime()')]
    original_map={field:(kind,function) for field,kind,function in re.findall(r'RobotCallbacks\.(\w+)\s*=\s*(TRobot\w+)\((Robot\w+)\);',block)}
    producer=(ROOT/'port/matrixgame/native/rangers_host_callbacks.cpp').read_text()
    native_map={field:(kind,function) for field,kind,function in re.findall(r'callbacks\.(\w+)\s*=\s*Robot::(TRobot\w+)\(Robot::(Robot\w+)\);',producer)}
    assert len(original_map)==16 and native_map==original_map
    assert not re.search(r'Windows(?:Imports|Sdk)::(?:LoadLibrary|GetProcAddress)',producer)
    check_hashes(sdk['link_inputs_sha256']);assert not sdk['strong_undefined'] and not sdk['new_host_code_reachable_in_probe']
    for name,value in sdk['artifacts'].items():assert sha(ROOT/name)==value['sha256']
    provenance=read(BASE/'sdk/core/source/source-provenance.json');assert provenance['upstream_pin']=='218115a72fa9c5d43269f4ca84a8596a0bcb82f1'
    check_hashes(provenance['outputs_sha256'],BASE/'sdk/core/source')
    protected_report=read(ROOT/'reports/matrixgame-e2e521-font-evidence.json')
    protected={name:value['sha256'] for name,value in protected_report['protected'].items() if value.get('status')!='UNAVAILABLE'}
    protected.update({name:value['sha256'] for name,value in protected_report['extra_frozen_artifacts_verified'].items()});check_hashes(protected)
    primary=protected_report['primary_NRO_verified_against_previous_fingerprint'];assert sha(Path(primary['path']))==primary['sha256']
    assert sha(ROOT/'Switch-E2E5-TEST.nro')==protected_report['root_cumulative_NRO_verified_against_documented_hash']
    previous_nro=ROOT/'build/matrixgame/e2e568-native-services/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro'
    assert sha(previous_nro)=='d17fb1d5b852b2e4f613f7c668f98ebbafe734780f2a510624eb881a187a2750'
    git='C:/Program Files/Git/cmd/git.exe';pins={}
    for name in ('matrixgame','cpp'):
        folder=ROOT/'upstream'/name;pins[name]=subprocess.check_output([git,'-C',folder,'rev-parse','HEAD'],text=True).strip()
        assert not subprocess.check_output([git,'-C',folder,'status','--porcelain'],text=True),name
    assert pins['matrixgame']==provenance['upstream_pin']
    result={'status':'PASS; REAL RANGERS SYMBOLIC SOUND HOST INTEGRATION; FULL NATIVE HOST STILL OPEN','host':host,'asan_new_TUs_only':asan,'sdk':sdk,
        'independent_WAV_PCM':{'channels':1,'sample_bits':16,'rate':44100,'frames':3437,'bytes':len(pcm),'sha256':hashlib.sha256(pcm).hexdigest(),'exact':True},
        'callback_producer_matches_original_slots':native_map,'upstream_pins_clean':pins,'protected_verified':len(protected)+2,
        'latest_diagnostic_E568_NRO_preserved':sha(previous_nro),'new_NRO':False,
        'remaining':['real text/music/progress/render-release host dependencies','full Rangers SDK host dependency link and native entry wiring','native Init/map after ReadParams/ApplyGammaRamp','audible hardware, ARM64/QEMU audio runtime and Switch image/VRAM/HID/focus/recovery/F9'],
        'limits':'Original-game host objects reused by hash. Separate sanitizer profile instruments new probe/owner/producer/bridge and refreshed platform TUs only; 329 older original units are optimized and not ASan/UBSan instrumented. SDK new code compiled in archive; only existing bridge is reached by the link probe.',
        'Switch':'NOT RUN','commit':False,'push':False}
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(result['status']);print(json.dumps(result['independent_WAV_PCM'],indent=2))
if __name__=='__main__':main()
