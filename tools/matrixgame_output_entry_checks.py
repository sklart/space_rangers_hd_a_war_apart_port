"""Real original Init prefix with refusing caps fixture; verify output scope rollback."""
import argparse,hashlib,json,subprocess,sys,time
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e527-output'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['host','arm64']);kind=ap.parse_args().kind;out=BASE/('entry-'+kind);out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[]}
 def run(cmd,label,cwd=ROOT):
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=cwd,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'cwd':str(cwd),'exit':p.returncode,'seconds':time.monotonic()-start});print(label,p.returncode,(p.stdout+p.stderr)[-2400:],flush=True);assert p.returncode==0;return p.stdout
 try:
  prior=json.loads((ROOT/('build/matrixgame/e2e521/'+kind+'/result.json')).read_text());template=prior['commands'][0]
  sources=[ROOT/'port/matrixgame/native-engine'/n for n in ('init_runtime.cpp','original_game.cpp','platform_3g.cpp','output_paths_backend.cpp')]+[BASE/'source/MatrixGame/src/MatrixGame.cpp',ROOT/'tests/test_matrixgame_native_entry.cpp'];objects=[]
  for p in sources:
   cmd=['wsl','--exec']+[s.replace('/build/matrixgame/e2e521/source','/build/matrixgame/e2e527-output/source') for s in template if s not in ('-DNDEBUG','-O3')];cmd+=['-O1','-fno-pie'];obj=out/(p.stem+'.o');cmd[cmd.index('-c')+1]=linux(p);cmd[cmd.index('-o')+1]=linux(obj);run(cmd,'compile-'+p.stem);objects.append(linux(obj))
  previous=next(cmd for cmd in prior['commands'] if '-Wl,--gc-sections' in cmd);oldroot=linux(ROOT/('build/matrixgame/e2e519/vector-group-fresh-'+kind));suffix=previous[previous.index('-o')+2:];suffix=[oldroot+'/'+s if s.endswith('.a') and not s.startswith('/') else s for s in suffix]
  cmd=['wsl','--exec',previous[0],'-Wl,--gc-sections','-no-pie',*objects,linux(ROOT/('build/matrixgame/e2e519/skin-borrow-assertions-'+kind+'/callbacks.o')),'-o',linux(out/'native-entry'),*suffix];run(cmd,'link')
  original=json.loads((ROOT/('build/matrixgame/e2e519/original-init-'+kind+'/result.json')).read_text());cwd=original['commands'][-1]['cwd'];cmd=['wsl','--cd',cwd,'--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  j['stdout']=run(cmd+[linux(out/'native-entry')],'runtime');assert 'two original cleanup cycles/output unbind PASS' in j['stdout']
  sources += [ROOT/'port/matrixgame/native-engine'/n for n in ('init_runtime.hpp','original_game.hpp','output_paths_backend.hpp')]+[Path(__file__).resolve()];j['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sources};j['status']='PASS';j['full_native_Init']=False;j['Switch_hardware']='HARDWARE_PENDING';j['device_caps_fixture']='DELIBERATE REFUSAL, no GPU success'
 finally:(out/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()
