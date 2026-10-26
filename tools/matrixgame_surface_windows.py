"""Addressed full original Windows Init/frame after original texture cleanup change."""
import hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/matrixgame/e2e525-surfaces/windows-frame'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 OUT.mkdir(parents=True,exist_ok=True);prior=json.loads((ROOT/'build/matrixgame/e2e523-presentation/windows-frame/result.json').read_text());j={'status':'INCOMPLETE','commands':[]}
 def run(command,label,cwd=ROOT):
  script=OUT/(label+'.cmd');script.write_text(command+'\n');p=subprocess.run(['cmd.exe','/d','/c',script],cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace');(OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-2200:] if p.returncode else '',flush=True);assert p.returncode==0
 try:
  sources=[ROOT/'build/matrixgame/e2e525-surfaces/sdk/source/MatrixLib/3G'/n for n in ('OriginalUnmanagedTexture.cpp','OriginalShadowTexture.cpp')]+[ROOT/'port/matrixgame/native-engine/offscreen_scope.cpp']
  objects=[]
  for source in sources:
   obj=OUT/(source.stem+'.obj');objects.append(obj);command=prior['commands'][0]['command'].replace(str(ROOT/'port/matrixgame/native-engine/init_runtime.cpp'),str(source)).replace(str(ROOT/'build/matrixgame/e2e523-presentation/windows-frame/init_runtime.obj'),str(obj));run(command,'build-'+source.stem)
  link=next(c['command'] for c in prior['commands'] if c['label']=='link');link=link.replace(str(ROOT/'build/matrixgame/e2e523-presentation/windows-frame/windows-game.exe'),str(OUT/'windows-game.exe')).replace(str(ROOT/'build/matrixgame/e2e523-presentation/windows-frame/windows-game.map'),str(OUT/'windows-game.map'));link+=' '+' '.join('"'+str(p)+'"' for p in objects);run(link,'link',ROOT/'build/matrixgame/e2e519/original-init-windows')
  old=json.loads((ROOT/'build/matrixgame/e2e522/windows-frame/result.json').read_text());cmd=[str(OUT/'windows-game.exe'),str(OUT)];p=subprocess.run(cmd,cwd=old['cwd'],env=dict(os.environ,MATRIXGAME_UNION_OUTPUT=str(OUT)),capture_output=True,text=True,timeout=240);(OUT/'runtime.log').write_text(p.stdout+p.stderr);j['commands'].append({'label':'runtime','command':cmd,'exit':p.returncode});j['stdout']=p.stdout;print('runtime',p.returncode,(p.stdout+p.stderr)[-3000:],flush=True);assert p.returncode==0
  assert p.stdout.count('original CGame::Init returned: ready=1')==2 and p.stdout.count('actual SDK draws=1008')==2
  j['snapshots']={n:sha(OUT/n) for n in prior['snapshots']};assert j['snapshots']==prior['snapshots'];j['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sources+[ROOT/'tools/matrixgame_unmanaged_texture_overrides.py',ROOT/'tools/matrixgame_shadow_texture_overrides.py',ROOT/'port/matrixgame/native-engine/offscreen_scope.hpp',Path(__file__).resolve()]};j['status']='PASS'
 finally:(OUT/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()
