"""Rebuild the final service test only; retain verified production objects."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_native_services_checks import ROOT,BASE,sha,read,linux
def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('host','arm64'));kind=parser.parse_args().kind
    prior=read(BASE/kind/'result.json');assert prior['status'].startswith('PASS');assert all(c['exit']==0 for c in prior['commands'])
    out=BASE/(kind+'-final');out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'reused_production_objects_sha256':{}}
    test=ROOT/'tests/test_matrixgame_native_services.cpp'
    inputs={name:digest for name,digest in prior['source_sha256'].items() if Path(name).suffix!='.py' and ROOT/name!=test}
    for name,digest in inputs.items():assert sha(ROOT/name)==digest,name
    inputs[str(test.relative_to(ROOT))]=sha(test);inputs[str(Path(__file__).resolve().relative_to(ROOT))]=sha(Path(__file__).resolve())
    for command in prior['commands']:
        if command['label'].startswith('compile-') and command['label']!='compile-test_matrixgame_native_services':
            path=Path(command['command'][command['command'].index('-o')+1].replace('/mnt/d/','D:/'))
            result['reused_production_objects_sha256'][str(path.relative_to(ROOT))]=sha(path)
    def run(command,label,cwd=ROOT):
        command=list(map(str,command));p=subprocess.run(command,cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'cwd':str(cwd),'exit':p.returncode})
        print(kind,label,p.returncode,log[-5000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label
        return log
    try:
        command=next(c['command'] for c in prior['commands'] if c['label']=='compile-test_matrixgame_native_services')[:]
        old_object=command[command.index('-o')+1];obj=out/'native-services-test.o';command[command.index('-o')+1]=linux(obj)
        run(command,'compile-final-test')
        command=next(c['command'] for c in prior['commands'] if c['label']=='link')[:]
        assert old_object in command;command[command.index(old_object)]=linux(obj);command[command.index('-o')+1]=linux(out/'native-services')
        run(command,'link');runtime=next(c for c in prior['commands'] if c['label']=='runtime');command=runtime['command'][:];command[-1]=linux(out/'native-services')
        result['stdout']=run(command,'runtime',Path(runtime['cwd']));assert 'callback_failure_unbind=PASS' in result['stdout']
        for name,digest in inputs.items():assert sha(ROOT/name)==digest,name
        result['source_sha256']=inputs;result['archive_sha256']=prior['archive_sha256'];result['binary_sha256']=sha(out/'native-services')
        result['status']='PASS; FINAL SERVICE FIXTURE AND ORIGINAL INIT PREFIX';result['Switch']='NOT RUN'
    finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
