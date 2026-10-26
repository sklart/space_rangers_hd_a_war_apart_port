"""Rebuild only changed common runtime for original full Windows HAL regression."""
import hashlib,json,os,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/matrixgame/e2e523-presentation/windows-frame'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    OUT.mkdir(parents=True,exist_ok=True);prior=json.loads((ROOT/'build/matrixgame/e2e522/windows-frame/result.json').read_text());result={'status':'INCOMPLETE','commands':[]}
    def run(command,label,cwd=ROOT):
        script=OUT/(label+'.cmd');script.write_text(command+'\n');p=subprocess.run(['cmd.exe','/d','/c',script],cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace');(OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-2000:] if p.returncode else '',flush=True);assert p.returncode==0
    try:
        source=ROOT/'port/matrixgame/native-engine/init_runtime.cpp';old=ROOT/'build/matrixgame/e2e522/clean-sdk-release/source/MatrixGame/src/OriginalMapPrepare.cpp';obj=OUT/'init_runtime.obj'
        command=prior['commands'][0]['command'].replace(str(old),str(source)).replace(str(ROOT/'build/matrixgame/e2e522/windows-frame/OriginalMapPrepare.obj'),str(obj));run(command,'build')
        link=next(c['command'] for c in prior['commands'] if c['label']=='link');old_object='"'+str(ROOT/'build/matrixgame/e2e521/windows/init.obj')+'"';assert link.count(old_object)==1;link=link.replace(old_object,'')
        link=link.replace(str(ROOT/'build/matrixgame/e2e522/windows-frame/windows-game.exe'),str(OUT/'windows-game.exe')).replace(str(ROOT/'build/matrixgame/e2e522/windows-frame/windows-game.map'),str(OUT/'windows-game.map'));link+=' "'+str(obj)+'"';run(link,'link',ROOT/'build/matrixgame/e2e519/original-init-windows')
        command=[str(OUT/'windows-game.exe'),str(OUT)];p=subprocess.run(command,cwd=prior['cwd'],env=dict(os.environ,MATRIXGAME_UNION_OUTPUT=str(OUT)),capture_output=True,text=True,timeout=240);result['commands'].append({'label':'runtime','command':command,'exit':p.returncode});(OUT/'runtime.log').write_text(p.stdout+p.stderr);result['stdout']=p.stdout;print('runtime',p.returncode,(p.stdout+p.stderr)[-3500:],flush=True);assert p.returncode==0
        assert p.stdout.count('original CGame::Init returned: ready=1')==2 and p.stdout.count('actual SDK draws=1008')==2
        result['snapshots']={n:sha(OUT/n) for n in prior['snapshots']};assert result['snapshots']==prior['snapshots'];result['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in (source,ROOT/'port/matrixgame/native-engine/native_frame_scope.hpp',Path(__file__).resolve())};result['status']='PASS'
    finally:(OUT/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
