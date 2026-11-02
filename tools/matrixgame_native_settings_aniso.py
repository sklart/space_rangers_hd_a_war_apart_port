"""Original manager to actual native sampler validation and SDK sampler fields."""
import argparse
import json
import subprocess
from pathlib import Path
from matrixgame_native_settings_checks import ROOT,BASE,ENGINE,sha,linux


def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('baseline','host','arm64'));kind=parser.parse_args().kind
    target='arm64' if kind=='arm64' else 'host';out=BASE/('aniso-'+kind);out.mkdir(parents=True,exist_ok=True)
    source=ENGINE if kind=='baseline' else BASE/'source'
    sources=[ROOT/'tests/test_matrixgame_native_settings_aniso.cpp',source/'MatrixGame/src/MatrixSampleStateManager.cpp',ROOT/'port/matrixgame/native-engine/platform_3g.cpp']
    report={'status':'INCOMPLETE','commands':[],'source_sha256':{str(p.relative_to(ROOT)):sha(p) for p in sources}}
    def run(command,label):
        command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');report['commands'].append({'label':label,'command':command,'exit':p.returncode})
        print(kind,label,p.returncode,log[-5500:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label
        return log
    try:
        previous=json.loads((BASE/target/'result.json').read_text());command=previous['commands'][0]['command'][:]
        command=[s.replace(linux(ENGINE),linux(source)) for s in command]
        command[command.index(linux(ROOT/'tests/test_matrixgame_native_settings.cpp'))]=linux(sources[0])
        exe=out/'aniso';command[command.index('-o')+1]=linux(exe)
        command+=['-ffunction-sections','-fdata-sections','-Wl,--gc-sections',*map(linux,sources[1:])]
        if kind!='baseline':command+=['-DMATRIXGAME_SETTINGS_AF_CANDIDATE','-I/mnt/c/devkitPro/libnx/include']
        run(command,'build');runtime=previous['commands'][-1]['command'][:];runtime[-2:]=[linux(exe)]
        if kind=='baseline':runtime+=['--baseline']
        report['stdout']=run(runtime,'runtime');report['binary_sha256']=sha(exe)
        if kind!='baseline':report['source_sha256']['port/matrixgame/deko/sampler_deko.hpp']=sha(ROOT/'port/matrixgame/deko/sampler_deko.hpp')
        report['status']='PASS; BASELINE REPRODUCED' if kind=='baseline' else 'PASS; ORIGINAL MANAGER NATIVE SAMPLER CPU AND SDK FIELDS ONLY'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
