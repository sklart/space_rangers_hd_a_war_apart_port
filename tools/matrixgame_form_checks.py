"""Full original native form compile/link; no Switch runtime or NRO claim."""
import hashlib,json,os,re,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/e2e533-form'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=BASE/'sdk';out.mkdir(parents=True,exist_ok=True)
 paths=['tools/'+n for n in ('matrixgame_full_game_overrides.py','matrixgame_form_overrides.py','matrixgame_form_checks.py','matrixgame_device_state_overrides.py')]
 paths+=['port/matrixgame/native-engine/'+n for n in ('CMakeLists.txt','native_form_backend.hpp','native_form_backend.cpp','platform_switch.hpp','platform_switch.cpp','native_input_switch.hpp','native_input_switch.cpp','native_loop.hpp','native_loop.cpp','native_loop_switch.hpp','native_loop_switch.cpp')]
 paths+=['tests/probe_matrixgame_native_entry.cpp']+['port/matrixgame/native-engine/'+n for n in ('native_form_session.cpp','device_state_backend.hpp','platform_win32.hpp','platform_win32.cpp')];before={p:sha(ROOT/p) for p in paths}
 j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING','NRO_created':False}
 def run(cmd,label):
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64'),capture_output=True,text=True,encoding='utf-8',errors='replace')
  (out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8');j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start})
  print(label,p.returncode,(p.stdout+p.stderr)[-10000:] if label!='symbols' else 'saved',flush=True)
  if p.returncode:raise RuntimeError(label+' failed; inspect saved log')
  return p.stdout
 try:
  cmd=json.loads((ROOT/'build/matrixgame/e2e532-simulation/sdk/result.json').read_text())['commands'][0]['command'];cmd[cmd.index('-B')+1]=str(out)
  run(cmd,'configure');run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build')
  elf=out/'matrixgame-native-entry.elf';nm=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols')
  j['strong_undefined']=[s for s in nm.splitlines() if ' U ' in s];assert not j['strong_undefined']
  for name in ('CFormMatrixGame::Enter()', 'CFormMatrixGame::Draw()', 'CFormMatrixGame::Takt(int)', 'CFormMatrixGame::Keyboard(bool, unsigned char)', 'CFormMatrixGame::MouseKey(', 'CFormMatrixGame::SystemEvent(', 'vtable for CFormMatrixGame','RunOriginalNativeLoop()','native_loop::SwitchBackend::Draw('):assert name in nm,name
  ninja=next(v.split('=',1)[1] for v in cmd if v.startswith('-DCMAKE_MAKE_PROGRAM='))
  graph=run([ninja,'-C',out,'-t','graph','matrixgame-native-entry.elf'],'link-graph');inputs=set()
  for label in re.findall(r'\[label="((?:\\.|[^"\\])*)"',graph):
   p=(out/Path(json.loads('"'+label+'"'))).resolve()
   if p.is_relative_to(ROOT) and p.suffix in ('.o','.obj','.a') and p.is_file():inputs.add(p)
  assert inputs
  j['elf_sha256']=sha(elf);j['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in sorted(inputs)}
  j['link_input_scope']='own object/archive nodes reachable from ELF in Ninja graph; external SDK libraries not counted'
  j['status']='COMPILE/LINK PASS; RUNTIME NOT RUN'
 finally:
  j['source_sha256']={p:sha(ROOT/p) for p in paths};assert before==j['source_sha256'],'inputs changed during checks'
  (out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
