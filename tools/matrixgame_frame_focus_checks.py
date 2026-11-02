"""Reproduce focus/readback reentry with exact AcquirePrimary/FlushRecording, not a GPU emulator."""
import argparse,hashlib,json,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e553-frame-focus'
SOURCES=['tests/test_matrixgame_frame_focus.cpp','port/matrixgame/native-engine/applet_window_lifecycle.cpp',
 'port/matrixgame/native-engine/native_device_state.cpp','port/matrixgame/native-core/buffer_resources.cpp',
 'port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/material_combiner.cpp']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);report={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 source=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text();a=source.index('    void AcquirePrimary(){');b=source.index('\n    }',a)+6;candidate=source[a:b]+'\n';baseline=(BASE/'baseline-acquire.inc').read_text()
 assert 'window.WaitForDrawable();' in baseline and 'window.WaitForDrawable();' not in candidate
 assert 'frame.RequireBatch();' in candidate
 # Admission must process all lifecycle hooks before publishing a live frame.
 start=source[source.index('    void StartRecording(){'):a]
 assert start.index('window.WaitForDrawable();')<start.index('frame.Acquire();')<start.index('state.Frame(true);')
 for name,text in [('baseline',baseline),('candidate',candidate)]:
  (out/(name+'-acquire.inc')).write_text(text.replace('void AcquirePrimary()','void '+name.title()+'()'),encoding='utf-8')
 a=source.index('    void FlushRecording(){');b=source.index('\n    }',a)+6
 (out/'flush-recording.inc').write_text(source[a:b]+'\n',encoding='utf-8')
 def run(cmd,label):
  start=time.monotonic();proc=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=proc.stdout+proc.stderr
  report['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':proc.returncode,'seconds':time.monotonic()-start});(out/(label+'.log')).write_text(log,encoding='utf-8');print(kind,label,proc.returncode,log[-3500:] if proc.returncode or label=='runtime' else '',flush=True);assert proc.returncode==0,label
  if label=='runtime':report['stdout']=log
 try:
  flags=['-std=c++20','-O1','-g','-UNDEBUG','-ffp-contract=off','-fno-pie','-no-pie']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  includes=[out,*[ROOT/'port/matrixgame'/n for n in ('native-engine','native-core','deko')]];exe=out/'frame-focus-test'
  run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],*[linux(ROOT/p) for p in SOURCES],'-o',linux(exe)],'build')
  command=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':command+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  run(command+[linux(exe)],'runtime');report.update(status='PASS; BASELINE FOCUS READBACK REENTRY REPRODUCED; CPU ENDPOINTS ONLY',binary_sha256=sha(exe))
 finally:
  names=sorted(set(SOURCES+['tools/matrixgame_frame_focus_checks.py','port/matrixgame/native-engine/platform_switch.cpp','port/matrixgame/native-engine/native_frame_scope.hpp','port/matrixgame/native-engine/applet_window_lifecycle.hpp','port/matrixgame/native-engine/native_device_state.hpp']))
  report['source_sha256']={p:sha(ROOT/p) for p in names};report['extracted_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.glob('*.inc')};(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
