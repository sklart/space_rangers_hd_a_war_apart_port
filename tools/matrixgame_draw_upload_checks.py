"""Native draw upload bytes/addresses/ownership using SDK handles and CPU endpoints."""
import argparse,hashlib,json,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e554-draw-upload'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);report={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 baseline=(BASE/'baseline-platform.cpp').read_text();a=baseline.index('std::uint32_t AllocationSize(');b=baseline.index('\n}',a)+2
 (out/'allocation-size.inc').write_text(baseline[a:b]+'\n',encoding='utf-8');(out/'baseline-upload.inc').write_bytes((BASE/'baseline-upload.inc').read_bytes())
 def run(cmd,label):
  start=time.monotonic();proc=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=proc.stdout+proc.stderr
  report['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':proc.returncode,'seconds':time.monotonic()-start});(out/(label+'.log')).write_text(log,encoding='utf-8');print(kind,label,proc.returncode,log[-3500:] if proc.returncode or label=='runtime' else '',flush=True);assert proc.returncode==0,label
  if label=='runtime':report['stdout']=log
 try:
  flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  exe=out/'draw-upload-test'
  run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,'-I'+linux(out),'-I'+linux(ROOT/'port/matrixgame/native-engine'),'-I/mnt/c/devkitPro/libnx/include',linux(ROOT/'tests/test_matrixgame_draw_upload.cpp'),'-o',linux(exe)],'build')
  command=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':command+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  run(command+[linux(exe)],'runtime');report.update(status='PASS; EXACT BASELINE PAYLOAD; FEWER GPU BLOCK REQUESTS IN CPU SDK MODEL',binary_sha256=sha(exe))
 finally:
  names=['tests/test_matrixgame_draw_upload.cpp','tools/matrixgame_draw_upload_checks.py','port/matrixgame/native-engine/native_draw_upload.hpp','port/matrixgame/native-engine/platform_switch.cpp']
  report['source_sha256']={p:sha(ROOT/p) for p in names};report['extracted_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.glob('*.inc')}
  report['baseline_source_sha256']=sha(BASE/'baseline-platform.cpp');report['SDK_headers_sha256']={str(p):sha(p) for p in [Path('C:/devkitPro/libnx/include/deko3d.h'),Path('C:/devkitPro/libnx/include/deko3d.hpp')]}
  (out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
