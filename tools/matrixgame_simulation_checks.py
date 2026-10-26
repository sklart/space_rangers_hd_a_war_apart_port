"""Retain the actual original form Takt and its full simulation dependencies."""
import hashlib,json,os,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/matrixgame/e2e532-simulation'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 out=BASE/'sdk';out.mkdir(parents=True,exist_ok=True)
 paths=['tools/matrixgame_full_game_overrides.py','tools/matrixgame_simulation_checks.py',
        'port/matrixgame/native-engine/CMakeLists.txt','tests/probe_matrixgame_native_entry.cpp']
 before={p:sha(ROOT/p) for p in paths}
 j={'status':'INCOMPLETE','commands':[],'Switch_hardware':'NOT RUN / HARDWARE_PENDING',
    'native_Init_executed':False,'native_Takt_executed':False,'NRO_created':False}
 def run(cmd,label):
  start=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=ROOT,env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64'),capture_output=True,text=True,encoding='utf-8',errors='replace')
  (out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
  j['commands'].append({'label':label,'command':list(map(str,cmd)),'exit':p.returncode,'seconds':time.monotonic()-start})
  print(label,p.returncode,(p.stdout+p.stderr)[-8000:] if label!='symbols' else 'saved',flush=True)
  if p.returncode:raise RuntimeError(label+' failed; see saved log')
  return p.stdout
 try:
  cmd=json.loads((ROOT/'build/matrixgame/e2e531-stencil/sdk/result.json').read_text())['commands'][0]['command']
  cmd[cmd.index('-B')+1]=str(out);run(cmd,'configure')
  run([cmd[0],'--build',out,'--target','matrixgame-native-entry','-j','4'],'build')
  elf=out/'matrixgame-native-entry.elf';symbols=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols')
  j['strong_undefined']=[line for line in symbols.splitlines() if ' U ' in line];assert not j['strong_undefined']
  for name in ('TaktOriginalNativeGame(int)','CMatrixMap::Takt(int)','CGame::Init('):assert name in symbols,name
  j['elf_sha256']=sha(elf);j['link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in out.rglob('*') if p.is_file() and p.suffix in ('.o','.obj','.a')}
  assert not list(BASE.rglob('*.nro'))
  j['status']='COMPILE/LINK PASS; RUNTIME NOT RUN'
 finally:
  j['source_sha256']={p:sha(ROOT/p) for p in paths};assert before==j['source_sha256'],'inputs changed during build'
  (out/'result.json').write_text(json.dumps(j,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
