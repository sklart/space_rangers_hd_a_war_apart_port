"""Workspace-only native MSVC static SDL dummy/Ogg/Vorbis dependencies."""
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,CMAKE,NINJA
BASE=ROOT/'build/matrixgame/e2e577-rangers-windows/deps'
def main():
 BASE.mkdir(parents=True,exist_ok=True);env=environment(BASE);prefix=BASE/'prefix';result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
 old=read(ROOT/'build/matrixgame/e2e576-rangers-arm64/deps/result.json')
 for value in old['archives']:assert sha(ROOT/value['path'])==value['sha256']
 source_root=ROOT/'build/matrixgame/e2e576-rangers-arm64/deps/source'
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  logpath=BASE/(label+'.log');attempt=1
  while logpath.exists():logpath=BASE/f'{label}-{attempt}.log';attempt+=1
  logpath.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(logpath.relative_to(ROOT))});print(label,p.returncode,log[-8000:] if p.returncode else '',flush=True);assert not p.returncode,label
 try:
  for name,folder in [('ogg','libogg-1.3.6'),('vorbis','libvorbis-1.3.7'),('sdl','SDL2-2.32.10')]:
   command=[CMAKE,'-S',source_root/folder,'-B',BASE/name,'-G','Ninja','-DCMAKE_MAKE_PROGRAM='+str(NINJA),'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_INSTALL_PREFIX='+str(prefix),'-DCMAKE_POLICY_DEFAULT_CMP0091=NEW','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded','-DBUILD_SHARED_LIBS=OFF','-DBUILD_TESTING=OFF']
   # Installed MSVC's PDB service fails C1902 even on CMake's tiny /Zi
   # compiler check. These release-only reference libraries need no PDB.
   command+=['-DCMAKE_TRY_COMPILE_CONFIGURATION=Release','-DCMAKE_C_FLAGS_DEBUG=/Od /Ob0','-DCMAKE_CXX_FLAGS_DEBUG=/Od /Ob0']
   if name=='vorbis':command+=['-DOGG_INCLUDE_DIR='+str(prefix/'include'),'-DOGG_LIBRARY='+str(prefix/'lib/ogg.lib')]
   if name=='sdl':
    command+=['-DSDL_SHARED=OFF','-DSDL_STATIC=ON','-DSDL_TEST=OFF','-DSDL_TESTS=OFF','-DSDL_AUDIO=ON','-DSDL_DUMMYAUDIO=ON','-DSDL_DUMMYVIDEO=ON']
    command+=['-DSDL_'+feature+'=OFF' for feature in ('DIRECTX','WASAPI','WINDOWS','RENDER_D3D','RENDER_D3D11','RENDER_D3D12','OPENGL','OPENGLES','VULKAN','HIDAPI','SENSOR','DISKAUDIO')]
   run(command,'configure-'+name);run([CMAKE,'--build',BASE/name,'-j','4'],'build-'+name);run([CMAKE,'--install',BASE/name],'install-'+name)
  libraries=list((prefix/'lib').glob('*.lib'));assert len(libraries)>=4
  result['libraries_sha256']={str(p.relative_to(ROOT)):sha(p) for p in libraries};result['headers_sha256']={str(p.relative_to(ROOT)):sha(p) for p in (prefix/'include').rglob('*') if p.is_file()}
  result['archives']=old['archives'];result['status']='PASS; NATIVE WINDOWS STATIC DEPENDENCIES; REAL SDL DUMMY; NO SYSTEM INSTALL'
 finally:
  result['script_sha256']=sha(Path(__file__).resolve());(BASE/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
