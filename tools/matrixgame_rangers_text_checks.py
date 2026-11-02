"""Real original AFT text, baseline null clip and native adapter differential."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_rangers_sound_checks import linux,GAME,HOST
BASE=ROOT/'build/matrixgame/e2e570-rangers-text'
def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('baseline','host','asan'));kind=parser.parse_args().kind
    out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
    prior=read(ROOT/'build/matrixgame/e2e569-rangers-sound/host/result.json');report=read(HOST/'report.json')
    producer=BASE/'producer-baseline.cpp' if kind=='baseline' else ROOT/'port/matrixgame/native/rangers_host_callbacks.cpp'
    sources=[ROOT/'tests/probe_matrixgame_rangers_text.cpp',ROOT/'port/matrixgame/native/rangers_bridge.cpp',producer,ROOT/'port/matrixgame/native/rangers_audio_session.cpp']
    headers=[ROOT/'port/matrixgame/native/rangers_host_callbacks.hpp',ROOT/'port/matrixgame/native/rangers_audio_session.hpp',ROOT/'port/matrixgame/native/rangers_bridge.hpp']
    inputs={str(p.relative_to(ROOT)):sha(p) for p in sources+headers+[Path(__file__).resolve()]}
    protected=[GAME/'INSTALL.TXT',GAME/'CFG/CacheData.dat',GAME/'DATA/common.pkg',GAME/'DATA/forms.pkg',GAME/'DATA/Sound.pkg',ROOT/'build/matrixgame/e2e569-rangers-sound/host/rangers-sound',ROOT/'build/matrixgame/e2e569-rangers-sound/sdk/libmatrixgame-engine-callbacks.a']
    before={str(p):sha(p) for p in protected};result['protected_before']=before
    def run(command,label,expected=0):
        command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode})
        print(kind,label,p.returncode,log[-5000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==expected,label
        return log
    try:
        baseline_expected=read(ROOT/'build/matrixgame/e2e569-rangers-sound/sdk/result.json')['source_sha256']['port/matrixgame/native/rangers_host_callbacks.cpp']
        assert sha(BASE/'producer-baseline.cpp')==baseline_expected
        check_hashes(prior['reused_original_objects_sha256']);check_hashes(prior['original_compiled_sources_sha256'])
        reused=dict(prior['reused_original_objects_sha256'])
        for name,digest in prior['new_objects_sha256'].items():
            if Path(name).stem in ('e2e4_quicksave_transaction','e2e515_perf'):reused[name]=digest
        check_hashes(reused);assert len(reused)==331
        result['reused_original_objects_sha256']=reused
        sanitizer=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if kind=='asan' else []
        flags=report['compile_flags']+['-UNDEBUG','-I'+linux(ROOT/'port/matrixgame/native')]+sanitizer
        objects=[]
        for source in sources:
            obj=out/(source.stem+'.o');run(['wsl','--exec','g++',*flags,'-c',linux(source),'-o',linux(obj)],'compile-'+source.stem);objects.append(linux(obj))
        result['new_objects_sha256']={str(Path(name.replace('/mnt/d/','D:/')).relative_to(ROOT)):sha(Path(name.replace('/mnt/d/','D:/'))) for name in objects}
        manifest={'objects':objects+[linux(ROOT/name) for name in reused],'output':linux(out/'rangers-text'),'okgf':linux(HOST/'okgf/libokgf.so'),'link_report':linux(out/'link-report.json'),'link_flags':sanitizer}
        (out/'link-input.json').write_text(json.dumps(manifest,indent=2)+'\n');run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_rangers_sound_link.py'),linux(out/'link-input.json')],'link')
        fixture=out/'runtime';fixture.mkdir(exist_ok=True)
        command=['wsl','--exec','env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy']
        if kind=='asan':command+=['ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
        command+=[linux(out/'rangers-text'),linux(GAME),linux(fixture)]
        if kind=='baseline':command+=['--baseline-null']
        result['stdout']=run(command,'runtime',11 if kind=='baseline' else 0)
        if kind=='baseline':assert 'original_AFT_explicit_clip=PASS' in result['stdout']
        else:assert 'exact_original_pairs=24 crop=1 automatic=1 error_cleanup=PASS' in result['stdout']
        for name,digest in inputs.items():assert sha(ROOT/name)==digest,name
        result['binary_sha256']=sha(out/'rangers-text');result['output_sha256']={str(p.relative_to(ROOT)):sha(p) for p in fixture.rglob('*') if p.is_file()}
        result['status']='PASS; ACTUAL BASELINE NULL CLIP SEGFAULT REPRODUCED' if kind=='baseline' else 'PASS; ORIGINAL AFT RASTER DIFFERENTIAL; NO GPU'
        result['new_TUs_ASan_UBSan']=kind=='asan';result['reused_original_TUs_instrumented']=False
    finally:
        result['source_sha256']=inputs;result['protected_after']={str(p):sha(p) for p in protected};assert before==result['protected_after']
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
