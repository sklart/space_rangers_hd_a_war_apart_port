"""Real input routing/queue/aspect checks, host sanitizers and ARM64/QEMU."""
import hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(p):
 with p.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def main():
 parent=ROOT/'build/matrixgame/e2e609-rangers-input/cpu';parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();source=ROOT/'tests/test_matrixgame_rangers_input_queue.cpp';header=ROOT/'port/matrixgame/native-engine/rangers_canvas_pixels.hpp';result={'status':'INCOMPLETE','commands':[],'scope':'CPU INPUT/ASPECT/OWNERSHIP ONLY; HID/GPU NOT RUN','sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in (source,header,ROOT/'port/matrixgame/native-engine/native_ui_target.hpp',ROOT/'port/matrixgame/native-engine/rangers_input_queue.hpp',ROOT/'port/matrixgame/native-engine/native_input.hpp',ROOT/'port/matrixgame/native-engine/native_input.cpp',Path(__file__).resolve())}}
 def run(command,label):
  command=list(map(str,command));env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1');p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,timeout=120);log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log,flush=True);assert p.returncode==0,label
  if label.endswith('runtime'):assert 'exhaustive_letterbox_points=PASS' in log
 try:
  host=out/'pixels-asan';arm=out/'pixels-arm64';flags=['-std=c++20','-UNDEBUG','-g','-O1','-fno-omit-frame-pointer']
  run(['g++',*flags,'-fsanitize=address,undefined',source,ROOT/'port/matrixgame/native-engine/native_input.cpp','-pthread','-o',host],'host-compile');run([host],'host-runtime')
  run(['/usr/bin/aarch64-linux-gnu-g++',*flags,'-fsanitize=undefined',source,ROOT/'port/matrixgame/native-engine/native_input.cpp','-pthread','-o',arm],'arm64-compile');run(['qemu-aarch64','-L','/usr/aarch64-linux-gnu',arm],'arm64-runtime')
  for name,digest in result['sources_sha256'].items():assert sha(ROOT/name)==digest
  result.update(status='PASS HOST ASAN/UBSAN/LSAN AND ARM64 QEMU UBSAN; CPU INPUT ONLY',artifacts={str(p.relative_to(ROOT)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in (host,arm)})
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
