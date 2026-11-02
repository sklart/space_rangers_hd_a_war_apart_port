"""Test real SDK RAII plus extracted production command-owner member order."""
import argparse,json,subprocess,time
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e548-command-unwind'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/').replace('C:/','/mnt/c/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));args=ap.parse_args();kind=args.kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 source=ROOT/'port/matrixgame/native-engine/platform_switch.cpp';s=source.read_text();a=s.index('\n',s.index('    std::shared_ptr<native_texture::Texture> cursor_texture;'))+1;b=s.index('    static_assert(NativeCommandMemory<CommandDriver>::Alignment',a)
 candidate=s[a:b];baseline=(BASE/'baseline-fields.inc').read_text()
 assert baseline.index('dk::UniqueCmdBuf commands;')<baseline.index('dk::UniqueMemBlock command_memory')
 assert candidate.index('dk::UniqueMemBlock command_memory')<candidate.index('NativeCommandMemory<CommandDriver> command_pages')<candidate.index('dk::UniqueCmdBuf commands;')
 def run(cmd,label):
  started=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append(dict(label=label,command=list(map(str,cmd)),exit=p.returncode,seconds=time.monotonic()-started));print(label,p.returncode,log[-4000:] if p.returncode or label.endswith('runtime') else '',flush=True);assert p.returncode==0,label
  if label.endswith('runtime'):result[label]=log
 try:
  flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  binaries={}
  for mode,text in [('baseline',baseline),('candidate',candidate)]:
   folder=out/mode;folder.mkdir(exist_ok=True);(folder/'command-owner-fields.inc').write_text(text)
   exe=folder/'command-unwind-test'
   run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,'-I'+linux(folder),'-I'+linux(ROOT/'port/matrixgame/native-engine'),'-I/mnt/c/devkitPro/libnx/include',linux(ROOT/'tests/test_matrixgame_command_unwind.cpp'),'-o',linux(exe)],mode+'-build')
   cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   run(cmd+[linux(exe)]+(['baseline'] if mode=='baseline' else []),mode+'-runtime');binaries[mode]=sha(exe)
  result.update(status='PASS; BASELINE OWNERSHIP VIOLATION OBSERVED; NO GPU UAF CLAIM',binary_sha256=binaries)
 finally:
  result['source_sha256']={p:sha(ROOT/p) for p in ['tests/test_matrixgame_command_unwind.cpp','port/matrixgame/native-engine/platform_switch.cpp','port/matrixgame/native-engine/native_command_memory.hpp','tools/matrixgame_command_unwind_checks.py',str((BASE/'baseline-fields.inc').relative_to(ROOT))]}
  result['SDK_header_sha256']={str(p):sha(p) for p in [Path('C:/devkitPro/libnx/include/deko3d.hpp'),Path('C:/devkitPro/libnx/include/deko3d.h')]}
  (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
