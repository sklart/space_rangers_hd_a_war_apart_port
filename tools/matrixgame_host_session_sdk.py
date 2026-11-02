"""SDK link root for the owning RangersHostSession, using E573 dependencies."""
import json
import os
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
BASE=ROOT/'build/matrixgame/e2e574-rangers-host-session/sdk'
def main():
 BASE.mkdir(parents=True,exist_ok=True);result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','new_NRO':False}
 prior=read(ROOT/'build/matrixgame/e2e573-rangers-data/sdk-final/result.json');check_hashes(prior['dependency_objects_sha256'])
 archive=ROOT/'build/matrixgame/e2e573-rangers-data/sdk-final/dependencies/liboriginal-rangers-host-data-cleanup.a';assert sha(archive)==prior['dependency_archive_sha256']
 protected={name:value['sha256'] for name,value in prior['artifacts'].items()};check_hashes(protected)
 sources=[ROOT/'port/matrixgame/native'/name for name in ('CMakeLists.txt','engine_callbacks.cpp','engine_callbacks.hpp','rangers_audio_session.cpp','rangers_audio_session.hpp','rangers_music_session.cpp','rangers_music_session.hpp','rangers_data_session.cpp','rangers_data_session.hpp','rangers_host_session.cpp','rangers_host_session.hpp','rangers_host_callbacks.cpp')]+[ROOT/'tests/probe_matrixgame_rangers_host_session.cpp',Path(__file__).resolve()]
 before={str(path.relative_to(ROOT)):sha(path) for path in sources};env=dict(os.environ,DEVKITPRO='C:/devkitPro',DEVKITA64='C:/devkitPro/devkitA64')
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  (BASE/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-8000:] if p.returncode else '',flush=True);assert not p.returncode,label;return log
 try:
  command=next(c['command'] for c in prior['commands'] if c['label']=='configure')[:];command[command.index('-B')+1]=str(BASE)
  command+=['-DBRIDGE_HOST_LINK_PROBE_SOURCE='+str(ROOT/'tests/probe_matrixgame_rangers_host_session.cpp')]
  run(command,'configure');run([command[0],'--build',BASE,'--target','matrixgame-rangers-host-link-probe','-j','4'],'build')
  elf=BASE/'matrixgame-rangers-host-link-probe.elf';symbols=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-nm.exe','-C',elf],'symbols')
  result['strong_undefined']=[line for line in symbols.splitlines() if ' U ' in line];assert not result['strong_undefined']
  required=prior['required_linked_symbols']+['RangersHostSession::RangersHostSession(', 'EngineCallbacks::Bound(']
  for name in required:assert name in symbols,name
  result['required_linked_symbols']=required;result['elf_header']=run(['C:/devkitPro/devkitA64/bin/aarch64-none-elf-readelf.exe','-h',elf],'elf-header')
  assert 'AArch64' in result['elf_header'] and 'DYN (Position-Independent Executable file)' in result['elf_header']
  check_hashes(before);check_hashes(protected)
  result['artifacts']={str(path.relative_to(ROOT)):{'sha256':sha(path),'bytes':path.stat().st_size} for path in (elf,BASE/'libmatrixgame-engine-callbacks.a',BASE/'rangers-host-link.map')}
  result['link_inputs_sha256']={str(path.relative_to(ROOT)):sha(path) for path in BASE.rglob('*') if path.is_file() and path.suffix in ('.a','.obj','.o')}
  result['protected_E573_sha256']=protected;result['dependency_archive_sha256']=sha(archive)
  result['status']='PASS; OWNING RANGERS HOST SESSION FULL SDK LINK; NOT RUN'
 finally:
  result['source_sha256']=before;(BASE/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
