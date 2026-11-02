"""CPU checker bounds/rejection tests only; these do not render a GPU frame."""
import argparse,json,subprocess,time
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e544-smoke'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 def run(cmd,label):
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');j['commands'].append(dict(label=label,command=list(map(str,cmd)),exit=p.returncode,seconds=time.monotonic()-start));print(label,p.returncode,log[-3000:] if p.returncode or label=='runtime' else '',flush=True)
  if label=='runtime':j['stdout']=log
  assert p.returncode==0
 try:
  flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  exe=out/'pixels-test';run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,'-I'+linux(ROOT/'port/matrixgame/native-engine'),linux(ROOT/'tests/test_matrixgame_diagnostic_pixels.cpp'),'-o',linux(exe)],'build')
  cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  run(cmd+[linux(exe)],'runtime');j.update(status='PASS',binary_sha256=sha(exe))
 finally:
  j['source_sha256']={n:sha(ROOT/n) for n in ('tests/test_matrixgame_diagnostic_pixels.cpp','port/matrixgame/native-engine/native_diagnostic_pixels.hpp','tools/matrixgame_smoke_checks.py')};(out/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()
