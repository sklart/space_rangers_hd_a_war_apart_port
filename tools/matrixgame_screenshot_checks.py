"""Actual original CBitmap PNG/CFile with synthetic screenshot capture pixels."""
import argparse
import json
import subprocess
import uuid
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha
BASE=ROOT/'build/matrixgame/e2e567-screenshot'
ENGINE=ROOT/'build/matrixgame/e2e566-readback-abort/sdk/source'
def linux(path):return str(path).replace('\\','/').replace('D:/','/mnt/d/')
def main():
    parser=argparse.ArgumentParser();parser.add_argument('kind',choices=('host','arm64','windows'));kind=parser.parse_args().kind
    out=BASE/kind;out.mkdir(parents=True,exist_ok=True);report={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
    sources=[ROOT/'tests/test_matrixgame_screenshot.cpp',ROOT/'port/matrixgame/native-engine/native_screenshot.cpp',ROOT/'port/matrixgame/native-engine/output_paths_backend.cpp',ROOT/'port/matrixgame/native-engine/platform_3g.cpp',ENGINE/'MatrixGame/src/MatrixGamePathUtils.cpp']
    def run(command,label):
        command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
        (out/(label+'.log')).write_text(log,encoding='utf-8');report['commands'].append({'label':label,'command':command,'exit':p.returncode})
        print(kind,label,p.returncode,log[-5000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label
        return log
    try:
        fixture=out/('run-'+uuid.uuid4().hex)
        if kind=='windows':
            previous=ROOT/'build/matrixgame/e2e565-native-settings/config-windows'
            template=(previous/'build.cmd').read_text();marker=' "'+str(ROOT/'tests/test_matrixgame_native_settings_config.cpp')+'"'
            prefix=template[:template.index(marker)].replace(str(ROOT/'build/matrixgame/e2e564-pad-repeat/sdk/source'),str(ENGINE))
            libraries=read(previous/'result.json')['libraries_sha256'];report['archive_sha256']=libraries
            exe=out/'screenshot.exe'
            command=prefix+' /Gy /Gw /utf-8 '+' '.join('"'+str(p)+'"' for p in sources)+' /Fo"'+out.as_posix()+'/" /Fe"'+str(exe)+'" /link /OPT:REF '+' '.join('"'+name+'"' for name in libraries)+' shell32.lib ole32.lib user32.lib\n'
            script=out/'build.cmd';script.write_text(command);run(['cmd.exe','/d','/c',script],'build');runtime=[exe,fixture]
        else:
            prior=read(ROOT/f'build/matrixgame/e2e561-unmanaged-bitmap/{kind}/result.json')
            command=[s for s in prior['commands'][0]['command'] if not s.endswith('.cpp')]
            command=[s.replace(linux(ROOT/'build/matrixgame/e2e558-gamma/sdk/source'),linux(ENGINE)) for s in command]
            exe=out/'screenshot';command[command.index('-o')+1]=linux(exe)
            first=next(i for i,s in enumerate(command) if s.endswith('.a'));command[first:first]=list(map(linux,sources))
            run(command,'build');runtime=prior['commands'][-1]['command'][:];runtime[-3:]=[linux(exe),linux(fixture)]
            report['archive_sha256']={s:sha(Path(s.replace('/mnt/d/','D:/'))) for s in command if s.endswith('.a')}
        report['stdout']=run(runtime,'runtime');report['binary_sha256']=sha(exe)
        report['fixture']=str(fixture.relative_to(ROOT));report['output_sha256']={str(p.relative_to(ROOT)):sha(p) for p in fixture.rglob('*') if p.is_file()}
        report['observations_sha256']=sha(fixture/'pixels.bin');report['status']='PASS; ORIGINAL PNG AND OUTPUT PATH; SYNTHETIC CAPTURE NO GPU'
    finally:
        report['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sources+[ROOT/'port/matrixgame/native-engine/native_screenshot.hpp',Path(__file__).resolve()]}
        (out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
