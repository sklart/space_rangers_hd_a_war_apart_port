"""Addressed production command-memory protocol tests; never native GPU execution."""
import argparse,json,subprocess,time
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e540-command-memory'
def linux(path):return str(path).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 sources=['tests/test_matrixgame_native_command_memory.cpp','port/matrixgame/native-engine/native_command_memory.hpp','port/matrixgame/native-engine/native_draw_batch.hpp','port/matrixgame/native-engine/native_frame_scope.hpp','port/matrixgame/native-engine/platform_switch.cpp','tools/matrixgame_command_checks.py']
 def run(command,label):
  start=time.monotonic();p=subprocess.run(list(map(str,command)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append(dict(label=label,command=list(map(str,command)),exit=p.returncode,seconds=time.monotonic()-start));print(label,p.returncode,log[-3000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0
  if label=='runtime':result['stdout']=log
 try:
  compiler='g++' if kind=='host' else 'aarch64-linux-gnu-g++-15';flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  exe=out/'native-command-memory';run(['wsl','--exec',compiler,*flags,'-I'+linux(ROOT/'port/matrixgame/native-engine'),linux(ROOT/sources[0]),'-o',linux(exe)],'build')
  command=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':command+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  run(command+[linux(exe)],'runtime');result['binary_sha256']=sha(exe);result['status']='PASS'
 finally:
  result['source_sha256']={name:sha(ROOT/name) for name in sources};(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
