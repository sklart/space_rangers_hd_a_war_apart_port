"""Production batch/scene/owner policy under sanitizers and ARM64, not GPU execution."""
import argparse,json,subprocess,time
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,BASE,sha
def linux(path):return str(path).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 sources=['tests/test_matrixgame_native_draw_batch.cpp','port/matrixgame/native-engine/native_draw_batch.hpp','port/matrixgame/native-engine/native_frame_scope.hpp','port/matrixgame/native-engine/platform_switch.cpp','tools/matrixgame_batch_checks.py']
 captures=sorted((BASE/'baseline-windows').glob('frame-cost-*.csv'));assert len(captures)==2
 def run(command,label):
  start=time.monotonic();p=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append(dict(label=label,command=list(map(str,command)),exit=p.returncode,seconds=time.monotonic()-start));print(label,p.returncode,log[-2500:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0
  if label=='runtime':result['stdout']=log
 try:
  compiler='g++' if kind=='host' else 'aarch64-linux-gnu-g++-15';flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  exe=out/'native-batch';run(['wsl','--exec',compiler,*flags,'-I'+linux(ROOT/'port/matrixgame/native-engine'),linux(ROOT/sources[0]),'-o',linux(exe)],'build')
  command=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':command+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  run(command+[linux(exe),*map(linux,captures)],'runtime');result['binary_sha256']=sha(exe);result['status']='PASS'
 finally:
  result['source_sha256']={name:sha(ROOT/name) for name in sources};result['capture_sha256']={str(p.relative_to(ROOT)):sha(p) for p in captures};(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
