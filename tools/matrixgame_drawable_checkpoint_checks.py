"""Execute production foreground wait with real RunControl and deterministic clock/IPC."""
import argparse,json,subprocess,time
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e551-drawable-checkpoint'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 def run(cmd,label):
  started=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');j['commands'].append(dict(label=label,command=list(map(str,cmd)),exit=p.returncode,seconds=time.monotonic()-started));print(label,p.returncode,log[-3000:] if p.returncode or label.endswith('runtime') else '',flush=True);assert p.returncode==0,label
  if label.endswith('runtime'):j[label]=log
 try:
  flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  binaries={}
  for mode,source in [('baseline',BASE/'baseline-lifecycle.cpp'),('candidate',ROOT/'port/matrixgame/native-engine/applet_window_lifecycle.cpp')]:
   exe=out/(mode+'-checkpoint-test');run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,'-I'+linux(ROOT/'port/matrixgame/native-engine'),linux(ROOT/'tests/test_matrixgame_drawable_checkpoint.cpp'),linux(source),'-o',linux(exe)],mode+'-build')
   cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   run(cmd+[linux(exe)]+(['baseline'] if mode=='baseline' else []),mode+'-runtime');binaries[mode]=sha(exe)
  j.update(status='PASS; BASELINE CONTROLLED WAIT GAP REPRODUCED; CPU MODEL ONLY',binary_sha256=binaries)
 finally:
  paths=['port/matrixgame/native-engine/applet_window_lifecycle.cpp','port/matrixgame/native-engine/applet_window_lifecycle.hpp','port/matrixgame/native-engine/native_diagnostic_control.hpp','tests/test_matrixgame_drawable_checkpoint.cpp','tools/matrixgame_drawable_checkpoint_checks.py',str((BASE/'baseline-lifecycle.cpp').relative_to(ROOT))]
  j['source_sha256']={p:sha(ROOT/p) for p in paths};(out/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()
