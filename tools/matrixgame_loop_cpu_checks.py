"""Execute original native L3GRun with original Form/Reminder; CPU fixtures."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e533-form'
sys.stdout.reconfigure(encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=['host','arm64']);kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);source=BASE/'sdk/source'
 paths=[ROOT/p for p in ('tests/test_matrixgame_native_loop.cpp','port/matrixgame/native-engine/native_loop.cpp','port/matrixgame/native-engine/platform_3g.cpp','port/matrixgame/native-engine/platform_globals.cpp')]
 paths+=[source/p for p in ('MatrixLib/3G/OriginalNativeLoop.cpp','portable-core/Form.cpp','portable-core/CReminder.cpp')]
 tracked=paths+[ROOT/'port/matrixgame/native-engine/native_loop.hpp',Path(__file__).resolve()];before={str(p.relative_to(ROOT)):sha(p) for p in tracked}
 j={'status':'INCOMPLETE','commands':[],'scope':'actual original loop with CPU platform/form fixtures; NO MAP/GPU/HID'}
 def run(cmd,label):
  p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':cmd,'exit':p.returncode});print(label,p.returncode,(p.stdout+p.stderr)[-5000:],flush=True)
  if p.returncode:raise RuntimeError(label+' failed')
  return p.stdout
 try:
  includes=[ROOT/p for p in ('port/matrixgame/native-engine','port/matrixgame/native-core','port/matrixgame/third_party/directxmath/Inc','port/matrixgame/third_party/sal')]+[source/p for p in ('portable-core','MatrixLib/3G','MatrixLib/Base')]
  flags=['-std=c++20','-O1','-g','-UNDEBUG','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fno-pie','-no-pie','-pthread']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  exe=out/'original-loop';run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],*[linux(p) for p in paths],'-o',linux(exe)],'build')
  cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  j['stdout']=run(cmd+[linux(exe)],'runtime');j.update(status='PASS',binary_sha256=sha(exe))
 finally:
  j['source_sha256']={str(p.relative_to(ROOT)):sha(p) for p in tracked};assert before==j['source_sha256'];(out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
