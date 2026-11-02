"""Real data/package owner differential, rollback and borrowed full16 services."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_rangers_sound_checks import linux,GAME,HOST
from e2e_dat_cleanup import apply as dat_cleanup
BASE=ROOT/'build/matrixgame/e2e573-rangers-data'

def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('host','asan'));kind=parser.parse_args().kind
    out=BASE/(kind+'-final');out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
    prior=read(ROOT/f'build/matrixgame/e2e572-rangers-host-sdk/{kind}/result.json')
    shared=[ROOT/'port/matrixgame/native'/name for name in ('rangers_audio_session.cpp','rangers_music_session.cpp','rangers_data_session.cpp')]
    overlay_inputs={}
    overlay_outputs={}
    for unit in read(HOST/'report.json')['compiled_units']:
        name=Path(unit['original']).name
        if name not in ('EC_BlockPar.cpp','EC_Data.cpp'):continue
        source=ROOT/unit['effective'];overlay_inputs[str(source.relative_to(ROOT))]=sha(source)
        generated=out/'source'/name;generated.parent.mkdir(parents=True,exist_ok=True)
        generated.write_text(dat_cleanup(source.read_text(encoding='utf-8'),name),encoding='utf-8')
        overlay_outputs[str(generated.relative_to(ROOT))]=sha(generated);shared.append(generated)
    probes=[ROOT/'tests'/name for name in ('probe_matrixgame_rangers_data.cpp','probe_matrixgame_rangers_owned_host.cpp')]
    headers=[ROOT/'port/matrixgame/native'/name for name in ('rangers_data_session.hpp','rangers_audio_session.hpp','rangers_music_session.hpp')]
    before={str(path.relative_to(ROOT)):sha(path) for path in shared+probes+headers+[Path(__file__).resolve(),ROOT/'tools/e2e_dat_cleanup.py',ROOT/'tools/e2e_source_overrides.py']}
    core=ROOT/'build/matrixgame/e2e571-rangers-music/sdk-final/core/source'
    reused={name:value for name,value in prior['reused_objects_sha256'].items() if not name.endswith(('rangers_audio_session.o','rangers_music_session.o','EC_BlockPar.o','EC_Data.o'))}
    reused.update({name:value for name,value in prior['new_objects_sha256'].items() if name.endswith('engine_callbacks.o')})
    check_hashes(reused);assert len(reused)==332
    resources=[GAME/'INSTALL.TXT',GAME/'INSTALL_RUSSIAN.TXT',GAME/'INSTALL_ENGLISH.TXT',GAME/'CFG/Main.dat',GAME/'CFG/CacheData.dat',GAME/'CFG/Rus/Lang.dat',GAME/'CFG/Eng/Lang.dat']
    resources += [path for path in (GAME/'DATA').iterdir() if path.is_file()]
    resources += [path for path in (GAME/'Mods').rglob('*') if path.is_file()]
    protected={str(path):sha(path) for path in resources}
    def run(command,label):
        command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode})
        print(kind,label,p.returncode,log[-6000:] if p.returncode or label.startswith('run-') else '',flush=True);assert not p.returncode,label
        return log
    def link(objects,binary):
        manifest={'objects':[linux(path) for path in objects]+[linux(ROOT/name) for name in reused], 'output':linux(out/binary), 'okgf':linux(HOST/'okgf/libokgf.so'),'link_report':linux(out/(binary+'-link-report.json')),'link_flags':sanitizer}
        inp=out/(binary+'-link.json');inp.write_text(json.dumps(manifest,indent=2)+'\n')
        run(['wsl','--exec','python3',linux(ROOT/'tools/matrixgame_rangers_sound_link.py'),linux(inp)],'link-'+binary)
    def runtime(binary,dest,mode=None,lang='',skip=False,label=''):
        command=['wsl','--exec','env','SDL_AUDIODRIVER=dummy','SDL_VIDEODRIVER=dummy']
        if kind=='asan':command+=['ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
        command += [linux(out/binary),linux(GAME),linux(dest)]
        if mode:command += [mode,lang,'1' if skip else '0',label]
        return run(command,'run-'+label)
    try:
        sanitizer=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie'] if kind=='asan' else []
        flags=read(HOST/'report.json')['compile_flags']+['-UNDEBUG']+sanitizer
        flags += ['-I'+linux(path) for path in (ROOT/'port/matrixgame/native',ROOT/'port/matrixgame/native-core',core)]
        objects={}
        for source in shared+probes:
            obj=out/(source.stem+'.o');run(['wsl','--exec','g++',*flags,'-c',linux(source),'-o',linux(obj)],'compile-'+source.stem);objects[source.stem]=obj
        common=[objects[path.stem] for path in shared]
        link(common+[objects[probes[0].stem]],'rangers-data');link(common+[objects[probes[1].stem]],'owned-host')
        comparisons={}
        for case,lang,mods,skip in [('russian','','',False),('english','english','',False),('fallback','absent-language','',False),('mod','russian',r'Tweaks\SR2LoadingScreen',False),('skip','russian',r'Tweaks\SR2LoadingScreen',True)]:
            outputs={}
            for mode in ('reference','owned'):
                dest=out/case/mode;(dest/'config/Mods').mkdir(parents=True,exist_ok=True)
                (dest/'config/Mods/ModCFG.txt').write_text('CurrentMod='+mods+'\n')
                runtime('rangers-data',dest,mode,lang,skip,case+'-'+mode)
                outputs[mode]={name:sha(dest/name) for name in ('main.bin','language.bin','cache.bin','state.txt')}
            assert outputs['reference']==outputs['owned'],(case,outputs)
            comparisons[case]=outputs
        for case,text in [('package-error','Lang=Rus\nPackages {\nPackage=data\\e573-missing.pkg\n}\n'),('language-error','Lang=AbsentE573\nPackages {\n}\n')]:
            dest=out/case;(dest/'config').mkdir(parents=True,exist_ok=True)
            (dest/'config/INSTALL_RUSSIAN.TXT').write_text(text)
            runtime('rangers-data',dest,'failure','russian',False,case)
        result['owned_host_stdout']=runtime('owned-host',out/'owned-host-runtime',label='owned-host')
        assert 'OWNED_HOST_ENGINE full16=PASS text=PASS sound=PASS music_stream=PASS teardown=PASS' in result['owned_host_stdout']
        check_hashes(before);check_hashes(reused);check_hashes(protected)
        result['comparisons']=comparisons;result['new_objects_sha256']={str(path.relative_to(ROOT)):sha(path) for path in objects.values()}
        result['reused_objects_sha256']=reused;result['protected_before_after']=protected
        result['binaries']={name:sha(out/name) for name in ('rangers-data','owned-host')}
        result['new_TUs_instrumented']=kind=='asan';result['original_dependencies_instrumented']=False
        result['dat_cleanup_inputs_sha256']=overlay_inputs;result['dat_cleanup_outputs_sha256']=overlay_outputs
        result['status']='PASS; ORIGINAL CONFIG DIFFERENTIAL, ROLLBACK, BORROWED FULL16 SERVICES; SDL DUMMY ONLY'
    finally:
        result['source_sha256']=before;(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
