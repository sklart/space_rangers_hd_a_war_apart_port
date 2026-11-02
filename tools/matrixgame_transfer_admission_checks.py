"""Execute production texture_store/native_texture with explicit CPU SDK endpoints."""
import argparse,json,subprocess,time
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e550-transfer-admission'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/').replace('C:/','/mnt/c/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 def run(cmd,label,expected=0):
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append(dict(label=label,command=list(map(str,cmd)),exit=p.returncode,expected_exit=expected,seconds=time.monotonic()-start));print(label,p.returncode,log[-4000:] if p.returncode or label.endswith('runtime') else '',flush=True);assert p.returncode==expected,label
  if label.endswith('runtime'):result[label]=log
 try:
  flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie','-ffunction-sections','-fdata-sections','-Wl,--gc-sections']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  binaries={}
  for mode,store in [('baseline',BASE/'baseline-texture-store.cpp'),('candidate',ROOT/'port/matrixgame/deko/texture_store.cpp')]:
   exe=out/(mode+'-transfer-test')
   run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,'-I'+linux(ROOT/'port/matrixgame/deko'),'-I'+linux(ROOT/'port/matrixgame/native-engine'),'-I'+linux(ROOT/'port/matrixgame/native-core'),'-I/mnt/c/devkitPro/libnx/include',linux(ROOT/'tests/test_matrixgame_transfer_admission.cpp'),linux(store),linux(ROOT/'port/matrixgame/native-engine/native_texture.cpp'),'-o',linux(exe)],mode+'-build')
   cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
   if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
   run(cmd+[linux(exe)]+(['baseline'] if mode=='baseline' else []),mode+'-runtime');binaries[mode]=sha(exe)
   if kind=='host' and mode=='baseline':
    run(['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','LSAN_OPTIONS=exitcode=23',linux(exe),'baseline-leak'],'baseline-leak-runtime',expected=23)
    assert 'LeakSanitizer: detected memory leaks' in result['baseline-leak-runtime']
  result.update(status='PASS; BASELINE KNOWN LOST QUEUE DISPATCH REPRODUCED; CPU MODEL ONLY',binary_sha256=binaries)
 finally:
  result['source_sha256']={p:sha(ROOT/p) for p in ['tests/test_matrixgame_transfer_admission.cpp','port/matrixgame/deko/texture_store.cpp','port/matrixgame/deko/texture_store.hpp','port/matrixgame/native-engine/native_texture.cpp','port/matrixgame/native-engine/native_texture.hpp','port/matrixgame/native-engine/texture_residency.hpp','tools/matrixgame_transfer_admission_checks.py',str((BASE/'baseline-texture-store.cpp').relative_to(ROOT))]}
  result['SDK_headers_sha256']={str(p):sha(p) for p in [Path('C:/devkitPro/libnx/include/deko3d.h'),Path('C:/devkitPro/libnx/include/deko3d.hpp')]}
  (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
