"""Observe the actual original D3DX A8 render-texture negotiation on Windows HAL."""
import json,hashlib,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/matrixgame/e2e525-surfaces/windows-format'
def main():
 OUT.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[]}
 try:
  prior=(ROOT/'build/matrixgame/e2e524-text/windows/export.cmd').read_text();prior=prior.replace(str(ROOT/'tests/matrixgame_text_font_export.cpp'),str(ROOT/'tests/matrixgame_shadow_format_windows.cpp'));prior=prior.replace(' "'+str(ROOT/'port/matrixgame/native-engine/native_text.cpp')+'"','');prior=prior.replace('e2e524-text/windows','e2e525-surfaces/windows-format').replace('e2e524-text\\windows','e2e525-surfaces\\windows-format');script=OUT/'build.cmd';script.write_text(prior)
  for label,cmd in [('build',['cmd.exe','/d','/c',script]),('runtime',[OUT/'font-oracle.exe'])]:
   p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');(OUT/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-2400:],flush=True);assert p.returncode==0
   if label=='runtime':j['stdout']=p.stdout
  j['source_sha256']={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ('tests/matrixgame_shadow_format_windows.cpp','tools/matrixgame_surface_format.py')};j['status']='PASS'
 finally:(OUT/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()
