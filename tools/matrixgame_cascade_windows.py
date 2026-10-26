"""Focused Windows regression after adding NativeGraphics map dispatch.

Recompile only map_device_backend.cpp; retain the previous original Init/frame
closure and read-only resource directory. No release artifact is replaced.
"""
import hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521'
def main():
    out=BASE/'cascade/windows-engine';out.mkdir(parents=True,exist_ok=True)
    prior=json.loads((BASE/'windows/result.json').read_text());report={'commands':{},'results':{},'status':'INCOMPLETE'}
    def run(command,label,cwd=ROOT):
        script=out/(label+'.cmd');script.write_text(command+'\n');p=subprocess.run(['cmd.exe','/d','/c',str(script)],cwd=cwd,capture_output=True,text=True)
        report['commands'][label]=command;report['results'][label]=p.returncode;(out/(label+'.log')).write_text(p.stdout+p.stderr)
        print(label,p.returncode,(p.stdout+p.stderr)[-5000:] if p.returncode else '',flush=True)
        if p.returncode:raise RuntimeError(label)
    try:
        command=prior['commands']['map-device'].replace(str(BASE/'windows/map-device.obj'),str(out/'map-device.obj'))
        run(command,'map-device')
        command=prior['commands']['link'].replace(str(BASE/'windows/map-device.obj'),str(out/'map-device.obj'))
        command=command.replace(str(BASE/'windows/windows-game.exe'),str(out/'windows-game.exe')).replace(str(BASE/'windows/windows-game.map'),str(out/'windows-game.map'))
        run(command,'link',ROOT/'build/matrixgame/e2e519/original-init-windows')
        cmd=[str(out/'windows-game.exe'),str(out)];report['commands']['runtime']=cmd
        p=subprocess.run(cmd,cwd=Path(prior['cwd']),capture_output=True,text=True,timeout=180)
        report.update(runtime_exit=p.returncode,stdout=p.stdout,stderr=p.stderr,cwd=prior['cwd']);(out/'runtime.log').write_text(p.stdout+p.stderr)
        print('runtime',p.returncode,(p.stdout+p.stderr)[-7000:],flush=True);assert p.returncode==0
        report['snapshots']={}
        for name in ('map-frame-0.png','map-frame-1.png'):
            value=hashlib.sha256((out/name).read_bytes()).hexdigest();assert value==hashlib.sha256((BASE/'windows'/name).read_bytes()).hexdigest()
            report['snapshots'][name]=value
        assert p.stdout.count('original CGame::Init returned: ready=1')==2
        assert p.stdout.count('original frame returned; actual SDK draws=1008')==2
        report['status']='PASS'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
