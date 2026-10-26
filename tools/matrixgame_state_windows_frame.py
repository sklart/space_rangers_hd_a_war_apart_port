"""Focused real Windows Init/frame regression for shared buffer lifetime change."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e521'
def main():
    out=BASE/'state/windows-frame';out.mkdir(parents=True,exist_ok=True)
    prior=json.loads((BASE/'cascade/windows-engine/result.json').read_text());report={'status':'INCOMPLETE','commands':[]}
    def run(command,label,cwd=ROOT):
        script=out/(label+'.cmd');script.write_text(command+'\n');p=subprocess.run(['cmd.exe','/d','/c',script],cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace')
        report['commands'].append({'label':label,'command':command,'exit':p.returncode});(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
        print(label,p.returncode,(p.stdout+p.stderr)[-5000:] if p.returncode else '',flush=True)
        if p.returncode:raise RuntimeError(label)
    try:
        old=BASE/'cascade/windows-engine';source=ROOT/'port/matrixgame/native-core/buffer_resources.cpp';obj=out/'buffers.obj'
        command=prior['commands']['map-device'].replace(str(ROOT/'port/matrixgame/native-engine/map_device_backend.cpp'),str(source)).replace(str(old/'map-device.obj'),str(obj))
        run(command,'buffers')
        command=prior['commands']['link'].replace(str(old/'windows-game.exe'),str(out/'windows-game.exe')).replace(str(old/'windows-game.map'),str(out/'windows-game.map'))
        command+=' "'+str(obj)+'"';run(command,'link',ROOT/'build/matrixgame/e2e519/original-init-windows')
        p=subprocess.run([out/'windows-game.exe',out],cwd=Path(prior['cwd']),capture_output=True,text=True,timeout=180)
        report.update(runtime_exit=p.returncode,stdout=p.stdout,stderr=p.stderr,cwd=prior['cwd']);(out/'runtime.log').write_text(p.stdout+p.stderr)
        print('runtime',p.returncode,(p.stdout+p.stderr)[-5500:],flush=True);assert p.returncode==0
        assert p.stdout.count('original CGame::Init returned: ready=1')==2 and p.stdout.count('actual SDK draws=1008')==2
        report['snapshots']={}
        for name,h in prior['snapshots'].items():
            value=hashlib.sha256((out/name).read_bytes()).hexdigest();assert value==h;report['snapshots'][name]=value
        report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (source,source.with_suffix('.hpp'),Path(__file__).resolve())}
        report['status']='PASS'
    finally:(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
