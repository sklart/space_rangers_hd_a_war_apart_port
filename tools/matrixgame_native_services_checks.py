"""Current original Init prefix with scoped complete Rangers service table."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha
BASE=ROOT/'build/matrixgame/e2e568-native-services'
ENGINE=ROOT/'build/matrixgame/e2e567-screenshot/sdk/source'
def linux(path):return str(path).replace('\\','/').replace('D:/','/mnt/d/')
def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('host','arm64'));kind=parser.parse_args().kind
    out=BASE/kind;out.mkdir(parents=True,exist_ok=True);report={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
    sources=[ROOT/'port/matrixgame/native-engine'/name for name in ('init_runtime.cpp','original_game.cpp','platform_3g.cpp','output_paths_backend.cpp','native_diagnostic_journal.cpp')]
    sources+=[ROOT/'port/matrixgame/native-core/resource_paths.cpp',ENGINE/'MatrixGame/src/MatrixGame.cpp',ROOT/'tests/test_matrixgame_native_services.cpp']
    def run(command,label,cwd=ROOT):
        command=list(map(str,command));p=subprocess.run(command,cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');report['commands'].append({'label':label,'command':command,'cwd':str(cwd),'exit':p.returncode})
        print(kind,label,p.returncode,log[-5000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label
        return log
    try:
        prior=read(ROOT/f'build/matrixgame/e2e565-native-settings/entry-{kind}/result.json')
        old=linux(ROOT/'build/matrixgame/e2e564-pad-repeat/sdk/source')
        template=next(c['command'] for c in prior['commands'] if c['label']=='compile-init_runtime')
        objects=[]
        for source in sources:
            command=[s.replace(old,linux(ENGINE)) for s in template];obj=out/(source.stem+'.o')
            command[command.index('-c')+1]=linux(source);command[command.index('-o')+1]=linux(obj)
            run(command,'compile-'+source.stem);objects.append(linux(obj))
        command=next(c['command'] for c in prior['commands'] if c['label']=='link')[:]
        command=[s for s in command if not s.endswith('.o') or s.endswith('/callbacks.o')]
        command[command.index('-o')+1]=linux(out/'native-services');command[command.index('-o'):command.index('-o')]=objects
        report['archive_sha256']={s:sha(Path(s.replace('/mnt/d/','D:/'))) for s in command if s.endswith('.a')}
        run(command,'link');previous=next(c for c in prior['commands'] if c['label']=='runtime');runtime=previous['command'][:];runtime[-1]=linux(out/'native-services')
        report['stdout']=run(runtime,'runtime',Path(previous['cwd']));assert 'missing_callbacks_rejected_before_Init=16' in report['stdout']
        report['binary_sha256']=sha(out/'native-services');report['status']='PASS; COPIED SERVICE TABLE AND ORIGINAL INIT PREFIX ONLY'
    finally:
        names=[*sources,ROOT/'port/matrixgame/native-engine/native_rangers_services.hpp',ROOT/'port/matrixgame/native-engine/init_runtime.hpp',ROOT/'port/matrixgame/native-engine/original_game.hpp',Path(__file__).resolve()]
        report['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in names}
        (out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
