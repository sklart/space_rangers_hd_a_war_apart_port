"""Real Music.Robot/Ogg/stream and original headless host callback branches."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_rangers_sound_checks import linux,GAME,HOST
BASE=ROOT/'build/matrixgame/e2e571-rangers-music'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('host','asan'));kind=parser.parse_args().kind
    out=BASE/(kind+'-final');out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
    prior=read(ROOT/'build/matrixgame/e2e570-rangers-text/host/result.json');report=read(HOST/'report.json')
    sources=[ROOT/'tests/probe_matrixgame_rangers_music.cpp']+[ROOT/'port/matrixgame/native'/name for name in ('rangers_bridge.cpp','rangers_host_callbacks.cpp','rangers_audio_session.cpp','rangers_music_session.cpp')]
    headers=[ROOT/'port/matrixgame/native'/name for name in ('rangers_music_session.hpp','rangers_host_callbacks.hpp','rangers_audio_session.hpp','rangers_bridge.hpp')]
    inputs={str(p.relative_to(ROOT)):sha(p) for p in sources+headers+[Path(__file__).resolve()]}
    protected=[GAME/'INSTALL.TXT',GAME/'CFG/Main.dat',GAME/'CFG/Rus/Lang.dat',GAME/'CFG/CacheData.dat',GAME/'DATA/music.pkg',GAME/'DATA/Sound.pkg',ROOT/'build/matrixgame/e2e570-rangers-text/host/rangers-text',ROOT/'build/matrixgame/e2e570-rangers-text/sdk/libmatrixgame-engine-callbacks.a']
    before={str(p):sha(p) for p in protected};result['protected_before']=before
    def run(command,label):
        command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode})
        print(kind,label,p.returncode,log[-6000:] if p.returncode or label.startswith('runtime') else '',flush=True);assert p.returncode==0,label
        return log
    try:
        reused=prior['reused_original_objects_sha256'];check_hashes(reused);assert len(reused)==331;result['reused_original_objects_sha256']=reused
        check_hashes(read(ROOT/'build/matrixgame/e2e569-rangers-sound/host/result.json')['original_compiled_sources_sha256'])
        sanitizer=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if kind=='asan' else []
        flags=report['compile_flags']+['-UNDEBUG','-I'+linux(ROOT/'port/matrixgame/native')]+sanitizer;objects=[]
        for source in sources:
            obj=out/(source.stem+'.o');run(['wsl','--exec','g++',*flags,'-c',linux(source),'-o',linux(obj)],'compile-'+source.stem);objects.append(linux(obj))
        result['new_objects_sha256']={str(Path(name.replace('/mnt/d/','D:/')).relative_to(ROOT)):sha(Path(name.replace('/mnt/d/','D:/'))) for name in objects}
        manifest={'objects':objects+[linux(ROOT/name) for name in reused],'output':linux(out/'rangers-music'),'okgf':linux(HOST/'okgf/libokgf.so'),'link_report':linux(out/'link-report.json'),'link_flags':sanitizer}
        (out/'link-input.json').write_text(json.dumps(manifest,indent=2)+'\n');run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_rangers_sound_link.py'),linux(out/'link-input.json')],'link')
        command=['wsl','--exec','env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy']
        if kind=='asan':command+=['ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
        command+=[linux(out/'rangers-music'),linux(GAME),linux(out/'runtime')]
        result['stdout']=run(command,'runtime');assert 'real_Ogg_stream_consumed=PASS worker_teardown=PASS' in result['stdout']
        command[-1]=linux(out/'runtime-music-only')
        result['music_only_stdout']=run(command+['--music-only'],'runtime-music-only')
        assert 'real_Ogg_stream_consumed=PASS worker_teardown=PASS' in result['music_only_stdout']
        assert 'MODE sound_enabled=0 music_enabled=1 case_normalization=PASS playlist_membership=PASS' in result['music_only_stdout']
        for name,digest in inputs.items():assert sha(ROOT/name)==digest,name
        result['binary_sha256']=sha(out/'rangers-music');result['status']='PASS; ORIGINAL MUSIC CALLBACKS AND REAL STREAM; SDL DUMMY ONLY'
        result['new_TUs_ASan_UBSan']=kind=='asan';result['reused_original_TUs_instrumented']=False
    finally:
        result['source_sha256']=inputs;result['protected_after']={str(p):sha(p) for p in protected};assert before==result['protected_after']
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
