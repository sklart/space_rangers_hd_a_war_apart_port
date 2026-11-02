"""Fresh Windows x64 native MatrixGame archives for real hosted Init checks.

Builds production CMake targets only; no SDL/device emulation or fake services.
Keeps all prior x86/x64 reference libraries and diagnostic NROs read-only.
"""
import json
import subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha
from matrixgame_windows_host_env import environment,compiler,CMAKE,NINJA
BASE=ROOT/'build/matrixgame/e2e578-hosted-windows-native'
def main():
 BASE.mkdir(parents=True,exist_ok=True);env=environment(BASE)
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','runtime':'NOT RUN','new_NRO':False}
 images=ROOT/'build/matrixgame/e2e519/windows-x64';source=ROOT/'build/matrixgame/e2e518/arm64-normalized'
 libraries=[images/'png/Release/libpng16_static.lib',images/'jpeg/Release/jpeg-static.lib',images/'zlib/Release/zs.lib']
 result['image_inputs_sha256']={str(p.relative_to(ROOT)):sha(p) for p in libraries}
 nro=ROOT/'build/matrixgame/e2e575-hosted-diagnostic/sdk/diagnostic-nro/matrixgame-native-diagnostic.nro';result['NRO_before']=sha(nro)
 def run(command,label):
  command=list(map(str,command))
  if label=='configure':command.append('-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2')
  p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  path=BASE/(label+'.log');i=1
  while path.exists():path=BASE/f'{label}-{i}.log';i+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-10000:] if p.returncode else '',flush=True);assert not p.returncode;return log
 try:
  build=BASE/'native'
  run([CMAKE,'-S',ROOT/'port/matrixgame/native-engine','-B',build,'-G','Ninja','-DCMAKE_MAKE_PROGRAM='+str(NINJA),'-DCMAKE_CXX_COMPILER='+str(compiler(env)),'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_TRY_COMPILE_CONFIGURATION=Release','-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded','-DPython3_EXECUTABLE='+str(Path(__import__('sys').executable)),'-DBUILD_TESTING=OFF','-DCORE_BUILD_LINK_PROBE=OFF','-DCORE_SANITIZERS=OFF','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON','-DCMAKE_CXX_FLAGS=/EHsc /utf-8 /UNDEBUG /DNOMINMAX /I'+str(images/'png')+' /I'+str(images/'jpeg'),'-DPNG_PNG_INCLUDE_DIR='+str(source/'png-source'),'-DPNG_LIBRARY='+str(libraries[0]),'-DJPEG_INCLUDE_DIR='+str(source/'jpeg-source'),'-DJPEG_LIBRARY='+str(libraries[1]),'-DZLIB_INCLUDE_DIR='+str(ROOT/'build/e2e515/deps/zlib-1.3.2'),'-DZLIB_LIBRARY='+str(libraries[2]),'-DMATRIXGAME_D3DX9_ROOT='+str(ROOT/'build/matrixgame/e2e519/deps/microsoft.dxsdk.d3dx.9.29.952.8')],'configure')
  run([CMAKE,'--build',build,'--target','matrixgame-native-cache','-j','4'],'build')
  archives=list(build.rglob('*.lib'));assert len(archives)>=4
  result['archives_sha256']={str(p.relative_to(ROOT)):sha(p) for p in archives}
  result['generated']=read(build/'source/native-engine-source.json')
  result['compile_commands_sha256']=sha(build/'compile_commands.json')
  assert sha(nro)==result['NRO_before'];result['status']='PASS; FRESH MSVC X64 NATIVE MATRIXGAME PRODUCTION ARCHIVES; NO LINK/RUNTIME CLAIM'
 finally:
  result['script_sha256']=sha(Path(__file__).resolve());path=BASE/'build-result.json';i=1
  while path.exists():path=BASE/f'build-result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('result',path,flush=True)
if __name__=='__main__':main()
