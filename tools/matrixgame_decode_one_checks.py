"""Bit-exact FVF and actual extracted indexed gather; allocation counts, not FPS."""
import argparse,json,subprocess,time
from pathlib import Path
from matrixgame_draw_budget_checks import ROOT,sha
BASE=ROOT/'build/matrixgame/e2e552-vertex-decode'
def linux(p):return str(p).replace('\\','/').replace('D:/','/mnt/d/')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('kind',choices=('host','arm64'));kind=ap.parse_args().kind
 out=BASE/kind;out.mkdir(parents=True,exist_ok=True);j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING'}
 source=(ROOT/'port/matrixgame/native-engine/platform_switch.cpp').read_text();a=source.index('DeviceResult DekoDevice::NativeDrawIndexed(');b=source.index('\n}\n',a)+2;candidate=source[a:b]+'\n';baseline=(BASE/'baseline-indexed.inc').read_text()
 expected=baseline.replace('vertex.Decode(','vertex.DecodeOne(').replace('binding.stride).at(0);decoded','binding.stride);decoded');assert candidate==expected,'indexed body changed beyond DecodeOne call'
 (out/'baseline-gather.inc').write_text(baseline.replace('DekoDevice::NativeDrawIndexed','IndexedHarness::Baseline'))
 (out/'candidate-gather.inc').write_text(candidate.replace('DekoDevice::NativeDrawIndexed','IndexedHarness::Candidate'))
 (out/'native-primitive.inc').write_bytes((BASE/'native-primitive.inc').read_bytes())
 def run(cmd,label):
  started=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');j['commands'].append(dict(label=label,command=list(map(str,cmd)),exit=p.returncode,seconds=time.monotonic()-started));print(label,p.returncode,log[-4000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label
  if label=='runtime':j['stdout']=log
 try:
  flags=['-std=c++20','-O1','-g','-UNDEBUG','-fno-pie','-no-pie','-ffp-contract=off']
  if kind=='host':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  paths=['tests/test_matrixgame_vertex_decode_one.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/native-core/buffer_resources.cpp'];exe=out/'decode-one-test'
  includes=[out,ROOT/'port/matrixgame/deko',ROOT/'port/matrixgame/native-engine',ROOT/'port/matrixgame/native-core']
  run(['wsl','--exec','g++' if kind=='host' else 'aarch64-linux-gnu-g++-15',*flags,*['-I'+linux(p) for p in includes],'-I/mnt/c/devkitPro/libnx/include',*[linux(ROOT/p) for p in paths],'-o',linux(exe)],'build')
  cmd=['wsl','--exec','env','ASAN_OPTIONS=detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1']
  if kind=='arm64':cmd+=['qemu-aarch64','-L','/usr/aarch64-linux-gnu']
  run(cmd+[linux(exe)],'runtime');j.update(status='PASS; BIT EXACT CPU DECODE/GATHER; NO FPS CLAIM',binary_sha256=sha(exe))
 finally:
  names=['tests/test_matrixgame_vertex_decode_one.cpp','port/matrixgame/deko/fixed_vertex.cpp','port/matrixgame/deko/fixed_vertex.hpp','port/matrixgame/native-core/buffer_resources.cpp','port/matrixgame/native-core/buffer_resources.hpp','port/matrixgame/native-engine/staging_draw.hpp','port/matrixgame/native-engine/platform_switch.cpp','tools/matrixgame_decode_one_checks.py',str((BASE/'baseline-indexed.inc').relative_to(ROOT)),str((BASE/'native-primitive.inc').relative_to(ROOT))]
  j['source_sha256']={p:sha(ROOT/p) for p in names};j['extracted_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.glob('*.inc')};(out/'result.json').write_text(json.dumps(j,indent=2)+'\n')
if __name__=='__main__':main()
