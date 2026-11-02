"""Current original Init prefix, deliberate caps refusal, settings preflight."""
import argparse
import json
import subprocess
import time
from pathlib import Path
from matrixgame_native_settings_checks import ROOT,BASE,ENGINE,sha,linux


def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('host','arm64'));kind=parser.parse_args().kind
    out=BASE/('entry-'+kind);out.mkdir(parents=True,exist_ok=True)
    prior=json.loads((ROOT/f'build/matrixgame/e2e527-output/entry-{kind}/result.json').read_text())
    result={'status':'INCOMPLETE','commands':[],'device_caps':'DELIBERATE REFUSAL; NO GPU SUCCESS'}
    def run(command,label,cwd=ROOT):
        command=list(map(str,command));start=time.monotonic();p=subprocess.run(command,cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'cwd':str(cwd),'exit':p.returncode,'seconds':time.monotonic()-start})
        print(kind,label,p.returncode,log[-5500:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label
        return log
    try:
        template=next(c['command'] for c in prior['commands'] if c['label']=='compile-init_runtime')
        old_source=linux(ROOT/'build/matrixgame/e2e527-output/source')
        sources=[ROOT/'port/matrixgame/native-engine'/name for name in ('init_runtime.cpp','original_game.cpp','platform_3g.cpp','output_paths_backend.cpp','native_diagnostic_journal.cpp')]
        sources+=[ROOT/'port/matrixgame/native-core/resource_paths.cpp',ENGINE/'MatrixGame/src/MatrixGame.cpp',ROOT/'tests/test_matrixgame_native_entry.cpp']
        objects=[]
        for source in sources:
            command=[s.replace(old_source,linux(ENGINE)) for s in template];obj=out/(source.stem+'.o')
            command[command.index('-c')+1]=linux(source);command[command.index('-o')+1]=linux(obj)
            run(command,'compile-'+source.stem);objects.append(linux(obj))
        link=next(c['command'] for c in prior['commands'] if c['label']=='link')[:]
        link=[s for s in link if not s.endswith('.o') or s.endswith('/callbacks.o')]
        link[link.index('-o')+1]=linux(out/'native-entry')
        link[link.index('-o'):link.index('-o')]=objects
        core=ROOT/f'build/matrixgame/e2e559-config-tail/{kind}'/('build-native' if kind=='host' else 'build-make')/'core/libmatrixgame-native-core.a'
        link=[linux(core) if s.endswith('libmatrixgame-native-core.a') else s for s in link]
        result['archive_sha256']={s:sha(Path(s.replace('/mnt/d/','D:/'))) for s in link if s.endswith('.a')}
        run(link,'link')
        previous=next(c for c in prior['commands'] if c['label']=='runtime');runtime=previous['command'][:]
        runtime[-1]=linux(out/'native-entry');result['stdout']=run(runtime,'runtime',Path(previous['cwd']))
        assert 'provided_entry=1 rejected_before_Init=7 caller_unchanged=PASS' in result['stdout']
        assert 'two original cleanup cycles/output unbind PASS' in result['stdout']
        names=[*sources,ROOT/'port/matrixgame/native-engine/native_game_settings.hpp',ROOT/'port/matrixgame/native-engine/original_game.hpp']
        result['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in names}
        result['binary_sha256']=sha(out/'native-entry');result['status']='PASS; ORIGINAL INIT PREFIX AND EARLY SETTINGS REJECTION ONLY'
    finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
