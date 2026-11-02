"""Build two ordinary separate SDK NRO targets from the same verified closure."""
import json
import os
import subprocess
from pathlib import Path
import matrixgame_hosted_diagnostic_sdk as build
from matrixgame_standard_nro_audit import ROOT,read,sha
from matrixgame_diagnostic_package import verify_nro
BASE=ROOT/'build/matrixgame/e2e586-interactive-diagnostic/sdk'
def main():
 previous=ROOT/'build/matrixgame/e2e583-robot-orders/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';before=sha(previous);assert before=='53db0a16a8f187fc8dd50a55be20e2e3bd3335d6f86e80c02c13e164a9664233'
 build.BASE=BASE;build.TARGET='matrixgame-native-interactive';build.PACKAGE='interactive-nro';build.MAP='hosted-interactive.map';build.CONFIGURE_ARGS=['-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'];build.main()
 manual=read(BASE/'result.json');assert manual['status'].startswith('PASS');result={'status':'INCOMPLETE','interactive':manual,'commands':[],'Switch':'NOT RUN'}
 env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;path=BASE/(label+'.log');index=1
  while path.exists():path=BASE/f'{label}-{index}.log';index+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-7000:] if p.returncode else '',flush=True);assert not p.returncode;return log
 try:
  cmake=manual['commands'][0]['command'][0];run([cmake,'--build',BASE,'--target','matrixgame-native-diagnostic-nro','-j','4'],'quick-build')
  elf=BASE/'matrixgame-native-diagnostic.elf';symbols=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'quick-symbols');result['quick_strong_undefined']=[line for line in symbols.splitlines() if ' U ' in line];assert not result['quick_strong_undefined']
  result['quick_elf_header']=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],'quick-elf-header');assert 'AArch64' in result['quick_elf_header']
  for name in manual['required_linked_symbols']:assert name in symbols,name
  nro=BASE/'diagnostic-nro/matrixgame-native-diagnostic.nro';nacp=BASE/'diagnostic-nro/control.nacp'
  result['quick_nro_validation']=verify_nro(nro.read_bytes(),Path('C:/devkitPro/libnx/default_icon.jpg').read_bytes(),nacp.read_bytes())
  result['quick_artifacts']={str(p.relative_to(ROOT)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in (elf,nro,nacp,BASE/'hosted-diagnostic.map')}
  metadata=read(BASE/'native-diagnostic-build.json');assert metadata['diagnostic_variants']['matrixgame-native-interactive']['frame_target'] is None
  commands=read(BASE/'compile_commands.json');entries=[c for c in commands if c['file'].endswith('matrixgame_native_diagnostic_main.cpp')];assert len(entries)==2
  assert sum('MATRIXGAME_DIAGNOSTIC_MANUAL=1' in c['command'] for c in entries)==1
  result['entry_compile_commands']=entries
  fonts=[BASE/'debug-font/original-font.bin',*[BASE/f'text-font/font{i}.bin' for i in range(4)]]
  for name in ('matrixgame-native-interactive.elf','matrixgame-native-diagnostic.elf'):
   payload=(BASE/name).read_bytes()
   for font in fonts:assert font.is_file() and font.read_bytes() in payload,str(font)
  result['embedded_fonts_sha256']={str(p.relative_to(ROOT)):sha(p) for p in fonts}
  result['all_link_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in BASE.rglob('*') if p.is_file() and p.suffix in ('.a','.o','.obj')}
  assert sha(previous)==before;result['previous_NRO_preserved']=before
  result['status']='PASS; INTERACTIVE AND QUICK STANDARD SDK NRO TARGETS; ZERO STRONG UNDEFINED; NOT RUN'
 finally:
  result['script_sha256']=sha(Path(__file__).resolve());path=BASE/'variants-result.json';index=1
  while path.exists():path=BASE/f'variants-result-{index}.json';index+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(path,flush=True)
if __name__=='__main__':main()
