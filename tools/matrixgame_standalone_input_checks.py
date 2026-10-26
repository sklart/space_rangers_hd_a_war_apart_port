"""Compile/link actual standalone Npad setup; do not claim executed HID IPC."""
import hashlib,json,os,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e535-standalone-input'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=BASE/'sdk';out.mkdir(parents=True,exist_ok=True)
 paths=['tests/probe_matrixgame_native_entry.cpp']+['port/matrixgame/native-engine/'+n for n in ('native_input_switch.cpp','native_input_switch.hpp','native_input.cpp','native_input_engine.cpp')]
 before={p:sha(ROOT/p) for p in paths};j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING','NRO_created':False}
 def run(cmd,label):
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64'),capture_output=True,text=True,encoding='utf-8',errors='replace')
  (out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start})
  print(label,p.returncode,(p.stdout+p.stderr)[-4000:] if p.returncode else 'saved',flush=True)
  if p.returncode:j['status']='FAIL';raise RuntimeError(label+' failed')
  return p.stdout
 try:
  prior=read(ROOT/'build/matrixgame/e2e534-reset-owners/sdk/result.json');cmd=prior['commands'][0]['command'][:];cmd[cmd.index('-B')+1]=str(out)
  run(cmd,'configure');run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build')
  nmtool='C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe';symbols=run([nmtool,'-C',out/'matrixgame-native-entry.elf'],'symbols')
  j['strong_undefined']=[s for s in symbols.splitlines() if ' U ' in s];assert not j['strong_undefined']
  for s in ('CGame::Init(', 'CMatrixMap::PrepareMap(', 'CFormMatrixGame::Draw()', 'RunOriginalNativeLoop()', 'hidInitializeNpad', 'hidSetSupportedNpadIdType', 'hidSetSupportedNpadStyleSet'):assert s in symbols,s
  entry=next(out.rglob('probe_matrixgame_native_entry.cpp.obj'))
  refs=run([nmtool,'-C',entry],'entry-symbols')
  for s in ('hidInitializeNpad','hidSetSupportedNpadIdType','hidSetSupportedNpadStyleSet'):assert re.search(r'\bU '+s+r'\b',refs),s
  ninja=next(v.split('=',1)[1] for v in cmd if v.startswith('-DCMAKE_MAKE_PROGRAM='))
  graph=run([ninja,'-C',out,'-t','graph','matrixgame-native-entry.elf'],'link-graph');inputs=set()
  for label in re.findall(r'\[label="((?:\\.|[^"\\])*)"',graph):
   p=(out/Path(json.loads('"'+label+'"'))).resolve()
   if p.is_relative_to(ROOT) and p.suffix in ('.o','.obj','.a') and p.is_file():inputs.add(p)
  assert inputs
  generated=read(out/'source/native-engine-source.json')['files_sha256'];old=read(ROOT/'build/matrixgame/e2e534-reset-owners/sdk/source/native-engine-source.json')['files_sha256']
  assert generated==old and len(generated)==255
  for p,h in generated.items():assert sha(out/'source'/p)==h,p
  old_sources=read(ROOT/'build/matrixgame/e2e533-form/sdk/result.json')['source_sha256']
  for p in ('port/matrixgame/native-engine/native_input_switch.hpp','port/matrixgame/native-engine/native_input_switch.cpp'):assert sha(ROOT/p)==old_sources[p],p
  # Reverse only the new standalone-owned block and compare the complete entry
  # against the earlier compiled entry fingerprint. No original Init/loop edits.
  text=(ROOT/paths[0]).read_text();start=text.index('        // This process owns Npad policy.');end=text.index('        // This standalone process explicitly',start)
  assert hashlib.sha256((text[:start]+text[end:]).replace('\n','\r\n').encode()).hexdigest()==old_sources[paths[0]] or hashlib.sha256((text[:start]+text[end:]).encode()).hexdigest()==old_sources[paths[0]]
  j.update(status='COMPILE/LINK PASS; NATIVE HID RUNTIME NOT RUN',elf_sha256=sha(out/'matrixgame-native-entry.elf'),entry_sha256=sha(entry),
   link_inputs_sha256={str(p.relative_to(ROOT)):sha(p) for p in sorted(inputs)},original_generated_files_unchanged=len(generated),borrowed_input_provider_unchanged=True)
 finally:
  j['source_sha256']={p:sha(ROOT/p) for p in paths};assert before==j['source_sha256'],'inputs changed during build'
  (out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
