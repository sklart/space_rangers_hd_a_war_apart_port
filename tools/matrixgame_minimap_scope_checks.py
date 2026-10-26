"""Addressed production minimap lifetime scopes; explicitly CPU transfer driver."""
import json,subprocess,hashlib,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e528-minimap'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 kind=sys.argv[1];assert kind in ('host','arm64');out=BASE/('scopes-'+kind);out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[]}
 def run(cmd,label):
  start=time.monotonic();p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'seconds':time.monotonic()-start});print(label,p.returncode,(p.stdout+p.stderr)[-1800:],flush=True);assert p.returncode==0;return p.stdout
 try:
  sources=['tests/test_matrixgame_minimap_scopes.cpp']+['port/matrixgame/native-engine/'+p for p in ('native_surfaces.cpp','offscreen_backend.cpp','offscreen_scope.cpp','platform_3g.cpp')];cmd=json.loads((ROOT/('build/matrixgame/e2e522/queries-'+kind+'/result.json')).read_text())['commands'][0]['command'];cmd=[s for s in cmd if not s.endswith('.cpp')];first=next(i for i,s in enumerate(cmd) if s.endswith('.a'));cmd[first:first]=[linux(ROOT/p) for p in sources];cmd[cmd.index('-o')+1]=linux(out/'scopes');run(cmd,'build');cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1'];
  if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  j['stdout']=run(cmd+[linux(out/'scopes')],'runtime');j['source_sha256']={p:sha(ROOT/p) for p in sources+['port/matrixgame/native-engine/minimap_lifetime.hpp','port/matrixgame/native-engine/offscreen_scope.hpp','tests/test_matrixgame_native_surfaces.cpp','tools/matrixgame_minimap_scope_checks.py']};j['status']='PASS'
 finally:(out/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()
