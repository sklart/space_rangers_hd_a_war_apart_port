"""Standard separate libnx/deko NRO with live owning original Rangers services."""
import json
import os
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_diagnostic_package import verify_nro
BASE=ROOT/'build/matrixgame/e2e575-hosted-diagnostic/sdk'
TARGET='matrixgame-native-diagnostic'
PACKAGE='diagnostic-nro'
MAP='hosted-diagnostic.map'
CONFIGURE_ARGS=[]
HOST_DEPENDENCY_RESULT=ROOT/'build/matrixgame/e2e573-rangers-data/sdk-final/result.json'
HOST_ARCHIVE=ROOT/'build/matrixgame/e2e573-rangers-data/sdk-final/dependencies/liboriginal-rangers-host-data-cleanup.a'
def main():
 BASE.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
 dependencies=read(HOST_DEPENDENCY_RESULT);check_hashes(dependencies['dependency_objects_sha256'])
 archive=HOST_ARCHIVE;assert sha(archive)==dependencies['dependency_archive_sha256']
 old=read(ROOT/'build/matrixgame/e2e574-rangers-host-session/sdk/result.json')
 protected={name:value['sha256'] for name,value in old['artifacts'].items()};check_hashes(protected)
 old_nro=ROOT/'build/matrixgame/e2e568-native-services/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';protected[str(old_nro)]=sha(old_nro)
 protected['D:/logs2.7z']=sha(Path('D:/logs2.7z'))
 native=read(ROOT/'build/matrixgame/e2e568-native-services/sdk/result.json');command=native['commands'][0]['command'][:];command[command.index('-B')+1]=str(BASE)
 command += ['-DMATRIXGAME_DIAGNOSTIC_RANGERS_HOST=ON','-DBRIDGE_RANGERS_HOST_ARCHIVE='+str(archive),'-DMATRIXGAME_RANGERS_DEPENDENCY_MANIFEST='+str(HOST_DEPENDENCY_RESULT)]
 host_command=next(c['command'] for c in old['commands'] if c['label']=='configure')
 for prefix in ('-DBRIDGE_RANGERS_OKGF_ARCHIVE=','-DBRIDGE_RANGERS_SOFTFLOAT_ARCHIVE='):
  command += [next(c for c in host_command if c.startswith(prefix))]
 command+=CONFIGURE_ARGS
 env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
 def run(cmd,label):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  path=BASE/(label+'.log');attempt=1
  while path.exists():path=BASE/f'{label}-{attempt}.log';attempt+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-12000:] if p.returncode else '',flush=True);assert not p.returncode,label;return log
 try:
  run(command,'configure');run([command[0],'--build',BASE,'--target',TARGET+'-nro','-j','4'],'build')
  elf=BASE/(TARGET+'.elf');symbols=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols')
  result['strong_undefined']=[line for line in symbols.splitlines() if ' U ' in line];assert not result['strong_undefined']
  required=old['required_linked_symbols']+['CGame::Init(', 'NativeInitRuntime::InitializeNativeGame(', 'DekoDevice::']
  for name in required:assert name in symbols,name
  result['required_linked_symbols']=required
  result['elf_header']=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],'elf-header');assert 'AArch64' in result['elf_header']
  metadata=read(BASE/'native-diagnostic-build.json');assert metadata['rangers_host']['enabled'];check_hashes(metadata['own_source_sha256']);check_hashes(metadata['rangers_host']['archives_sha256'])
  assert sha(Path(metadata['rangers_host']['dependency_manifest']))==metadata['rangers_host']['dependency_manifest_sha256']
  nro=BASE/PACKAGE/(TARGET+'.nro');icon=Path('C:/devkitPro/libnx/default_icon.jpg');nacp=BASE/PACKAGE/'control.nacp'
  result['nro_validation']=verify_nro(nro.read_bytes(),icon.read_bytes(),nacp.read_bytes())
  assert b'B.RangersHostSession' in elf.read_bytes() and b'D.RangersHostSession' in elf.read_bytes()
  check_hashes(protected)
  result['artifacts']={str(path.relative_to(ROOT)):{'sha256':sha(path),'bytes':path.stat().st_size} for path in (elf,nro,nacp,BASE/MAP)}
  # A binary directory can hold both entries. A later build of the peer must
  # not invalidate this target's inventory with an object it never links.
  peers={'matrixgame-native-diagnostic','matrixgame-native-interactive'}-{TARGET}
  result['link_inputs_sha256']={str(path.relative_to(ROOT)):sha(path) for path in BASE.rglob('*') if path.is_file() and path.suffix in ('.a','.obj','.o') and not any('CMakeFiles/'+peer+'.dir/' in path.as_posix() for peer in peers)}
  result['entry_target']=TARGET
  result['protected_sha256']=protected;result['metadata_sha256']=sha(BASE/'native-diagnostic-build.json');result['build_id']=metadata['build_id']
  result['status']='PASS; JOINT NATIVE/DEKO + ORIGINAL RANGERS HOST SDK LINK AND STANDARD NRO; NOT RUN'
 finally:
  if (BASE/'result.json').exists():
   index=1
   while (BASE/f'result-attempt-{index}.json').exists():index+=1
   (BASE/f'result-attempt-{index}.json').write_bytes((BASE/'result.json').read_bytes())
  (BASE/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
