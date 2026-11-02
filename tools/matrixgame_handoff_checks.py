"""Addressed diagnostic handoff checks, using production Lifecycle/platform entry."""
import argparse,json,subprocess,time
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e546-window-handoff'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));ap.add_argument('--output',type=Path,default=BASE);args=ap.parse_args();kind=args.kind
 out=args.output.resolve()/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch_hardware':'CANDIDATE NOT RUN / HARDWARE_PENDING'}
 sources=['tests/test_matrixgame_diagnostic_window.cpp','port/matrixgame/native-engine/applet_window_lifecycle.cpp','port/matrixgame/native-engine/platform_3g.cpp']
 def run(cmd,label):
  started=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append(dict(label=label,command=list(map(str,cmd)),exit=p.returncode,seconds=time.monotonic()-started));print(label,p.returncode,log[-4000:] if p.returncode or label=='runtime' else '',flush=True)
  if label=='runtime':result['stdout']=log
  assert p.returncode==0,label
 try:
  flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  exe=out/'handoff-test';run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,'-I'+linux(ROOT/'port/matrixgame/native-engine'),'-I'+linux(ROOT/'port/matrixgame/native-core'),*[linux(ROOT/p) for p in sources],'-o',linux(exe)],'build')
  cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  run(cmd+[linux(exe)],'runtime');result.update(status='PASS',binary_sha256=sha(exe))
 finally:
  result['source_sha256']={p:sha(ROOT/p) for p in sources+['port/matrixgame/native-engine/native_diagnostic_window.hpp','port/matrixgame/native-engine/platform_3g.hpp','port/matrixgame/native-engine/applet_window_lifecycle.hpp','tools/matrixgame_handoff_checks.py']};(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
