"""Run the same real full16 engine-adapter probe on host, reusing E571 objects."""
import argparse
import json
import subprocess
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_rangers_sound_checks import linux,GAME,HOST
BASE=ROOT/'build/matrixgame/e2e572-rangers-host-sdk'

def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('host','asan'));kind=parser.parse_args().kind
    out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
    prior=read(ROOT/f'build/matrixgame/e2e571-rangers-music/{kind}-final/result.json')
    check_hashes(prior['source_sha256']);check_hashes(prior['reused_original_objects_sha256']);check_hashes(prior['new_objects_sha256'])
    sources=[ROOT/'tests/probe_matrixgame_rangers_host_link.cpp',ROOT/'port/matrixgame/native/engine_callbacks.cpp']
    headers=[ROOT/'port/matrixgame/native/engine_callbacks.hpp',ROOT/'port/matrixgame/native-core/config_unicode.hpp',ROOT/'port/matrixgame/native-core/utf16.hpp']
    before={str(path.relative_to(ROOT)):sha(path) for path in sources+headers}
    core=ROOT/'build/matrixgame/e2e571-rangers-music/sdk-final/core/source'
    generated={str(path.relative_to(ROOT)):sha(path) for path in core.glob('*.hpp')}
    def run(command,label):
        command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode})
        print(kind,label,p.returncode,log[-7000:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode,label
        return log
    protected=prior['protected_after'];check_hashes(protected)
    try:
        sanitizer=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if kind=='asan' else []
        flags=read(HOST/'report.json')['compile_flags']+['-UNDEBUG']+sanitizer
        flags += ['-I'+linux(path) for path in (ROOT/'port/matrixgame/native',ROOT/'port/matrixgame/native-core',core)]
        objects=[]
        for source in sources:
            obj=out/(source.stem+'.o');run(['wsl','--exec','g++',*flags,'-c',linux(source),'-o',linux(obj)],'compile-'+source.stem);objects.append(obj)
        reused={name:digest for name,digest in prior['new_objects_sha256'].items() if not name.endswith('probe_matrixgame_rangers_music.o')}
        reused.update(prior['reused_original_objects_sha256']);assert len(reused)==335
        result['reused_objects_sha256']=reused;result['new_objects_sha256']={str(path.relative_to(ROOT)):sha(path) for path in objects}
        manifest={'objects':[linux(path) for path in objects]+[linux(ROOT/name) for name in reused], 'output':linux(out/'rangers-host'), 'okgf':linux(HOST/'okgf/libokgf.so'),'link_report':linux(out/'link-report.json'),'link_flags':sanitizer}
        (out/'link-input.json').write_text(json.dumps(manifest,indent=2)+'\n')
        run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_rangers_sound_link.py'),linux(out/'link-input.json')],'link')
        command=['wsl','--exec','env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy']
        if kind=='asan':command+=['ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
        command += [linux(out/'rangers-host'),linux(GAME),linux(out/'runtime')]
        result['stdout']=run(command,'runtime');assert 'REAL_HOST_ENGINE full16=PASS text=PASS sound=PASS music_stream=PASS teardown=PASS' in result['stdout']
        check_hashes(before);check_hashes(generated);check_hashes(protected);check_hashes(reused)
        result['binary_sha256']=sha(out/'rangers-host');result['protected_verified']=protected
        result['new_TUs_instrumented']=kind=='asan';result['original_dependencies_instrumented']=False
        result['status']='PASS; FULL16 REAL ENGINE CALLBACKS HOST; SDL DUMMY ONLY'
    finally:
        result['source_sha256']=before;result['generated_headers_sha256']=generated
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
