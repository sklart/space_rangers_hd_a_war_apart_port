"""Real symbolic sound/cache/PCM via existing original-game objects, no GUI run."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e569-rangers-sound'
HOST=ROOT/'port/switch/build/e2e5-host-game/switch-effective'
GAME=ROOT/'windows/Space Rangers HD A War Apart'
def linux(path):return str(path).replace('\\','/').replace('D:/','/mnt/d/')
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--reuse-objects',action='store_true');parser.add_argument('--asan',action='store_true');args=parser.parse_args()
    out=BASE/('host-asan' if args.asan else 'host');out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
    previous=read(out/'result.json') if args.reuse_objects else None
    report=read(HOST/'report.json');sources=[ROOT/'tests/probe_matrixgame_rangers_sound.cpp']+[ROOT/'port/matrixgame/native'/name for name in ('rangers_bridge.cpp','rangers_host_callbacks.cpp','rangers_audio_session.cpp')]
    protected=[GAME/'CFG/CacheData.dat',GAME/'DATA/Sound.pkg',HOST/'original-game-host',HOST/'report.json']
    before={str(p):sha(p) for p in protected};result['protected_before']=before
    inputs={str(p.relative_to(ROOT)):sha(p) for p in sources+[ROOT/'port/matrixgame/native/rangers_audio_session.hpp',ROOT/'port/matrixgame/native/rangers_host_callbacks.hpp',Path(__file__).resolve(),ROOT/'tools/matrixgame_rangers_sound_link.py']}
    def run(command,label,cwd=ROOT):
        command=list(map(str,command));p=subprocess.run(command,cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'cwd':str(cwd),'exit':p.returncode})
        print(label,p.returncode,log[-5000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label
        return log
    try:
        original_sources=report['compiled_sources_sha256']
        changed=[name for name,digest in original_sources.items() if sha(ROOT/name)!=digest]
        refreshed=[entry for entry in report['compiled_units'] if entry['effective'] in changed and entry['original']!='tools/e2e5_host_game_main.cpp']
        assert {entry['effective'] for entry in refreshed}==set(changed),changed
        for entry in refreshed:
            source=ROOT/entry['effective'];sources.append(source);inputs[entry['effective']]=sha(source)
        result['refreshed_original_sources']=changed
        check_hashes({name:digest for name,digest in original_sources.items() if name not in changed})
        reused={entry['object']:entry['object_sha256'] for entry in report['compiled_units'] if entry['original']!='tools/e2e5_host_game_main.cpp' and entry['effective'] not in changed}
        check_hashes(reused);result['reused_original_objects_sha256']=reused
        result['original_compiled_sources_sha256']={name:sha(ROOT/name) for name in original_sources}
        sanitizer=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if args.asan else []
        objects=[];flags=report['compile_flags']+['-UNDEBUG','-I'+linux(ROOT/'port/matrixgame/native')]+sanitizer
        for source in sources:
            obj=out/(source.stem+'.o');command=['wsl','--exec','g++',*flags,'-c',linux(source),'-o',linux(obj)];label='compile-'+source.stem
            source_name=str(source.relative_to(ROOT));prior_command=next((c for c in previous['commands'] if c['label']==label),None) if previous else None
            previous_digest=(previous['source_sha256'].get(source_name) or previous['source_sha256'].get(source_name.replace('\\','/'))) if previous else None
            unchanged=previous and source!=sources[0] and obj.is_file() and prior_command and prior_command['exit']==0 and prior_command['command']==command and previous_digest==sha(source)
            unchanged=unchanged and all(previous['source_sha256'].get(name)==digest for name,digest in inputs.items() if name.endswith('.hpp'))
            if unchanged:
                result['commands'].append(dict(prior_command,reused_verified_object=True));print(label,'reused',flush=True)
            else:run(command,label)
            objects.append(linux(obj))
        result['new_objects_sha256']={str(Path(name.replace('/mnt/d/','D:/')).relative_to(ROOT)):sha(Path(name.replace('/mnt/d/','D:/'))) for name in objects}
        objects+=list(map(lambda name:linux(ROOT/name),reused))
        manifest={'objects':objects,'output':linux(out/'rangers-sound'),'okgf':linux(HOST/'okgf/libokgf.so'),'link_report':linux(out/'link-report.json'),'link_flags':sanitizer}
        assert sha(HOST/'okgf/libokgf.so')==report['okgf_library_sha256']
        (out/'link-input.json').write_text(json.dumps(manifest,indent=2)+'\n')
        run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_rangers_sound_link.py'),linux(out/'link-input.json')],'link')
        fixture=out/'runtime';fixture.mkdir(exist_ok=True)
        sanitizer_env=['ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1'] if args.asan else []
        result['stdout']=run(['wsl','--exec','env',*sanitizer_env,'SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy',linux(out/'rangers-sound'),linux(GAME),linux(fixture)],'runtime')
        result['backend_failure_stdout']=run(['wsl','--exec','env',*sanitizer_env,'SDL_AUDIODRIVER=matrixgame_missing_driver','SDL_VIDEODRIVER=dummy',linux(out/'rangers-sound'),linux(GAME),linux(out/'backend-failure'),'--backend-failure'],'backend-failure-runtime')
        result['new_TUs_ASan_UBSan']=args.asan;result['reused_original_TUs_instrumented']=False
        result['binary_sha256']=sha(out/'rangers-sound');result['output_sha256']={str(p.relative_to(ROOT)):sha(p) for p in fixture.rglob('*') if p.is_file()}
        for name,digest in inputs.items():assert sha(ROOT/name)==digest,name
        result['status']='PASS; REAL RANGERS SYMBOLIC SOUND SDL DUMMY; NO GPU NO INIT NO SWITCH'
    finally:
        result['source_sha256']=inputs;result['protected_after']={str(p):sha(p) for p in protected};assert before==result['protected_after']
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
