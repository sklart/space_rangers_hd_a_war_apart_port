"""Compile current native host API/real runtime with MSVC before big reference build."""
import json
import subprocess
from e2e_runtime_overrides import prepare_runtime_overlay
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,sha
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e577-rangers-windows'
def main():
 BASE.mkdir(parents=True,exist_ok=True);env=environment(BASE);result={'status':'INCOMPLETE','commands':[]}
 overlay=BASE/'runtime-overlay';overlay.mkdir(exist_ok=True)
 prepare_runtime_overlay(ROOT/'upstream/cpp/runtime',overlay,portable_windows_host=True)
 includes=[overlay,ROOT/'port/switch/platform',ROOT/'port/switch/build/e2e-game-perf515/source-overlay',ROOT/'upstream/cpp/src',ROOT/'upstream/cpp/runtime',ROOT/'port/matrixgame/native',ROOT/'port/matrixgame/native-core',ROOT/'build/matrixgame/e2e571-rangers-music/sdk-final/core/source']
 flags=['/nologo','/std:c++20','/EHsc','/utf-8','/MT','/O2','/UNDEBUG','/DNOMINMAX','/D_CRT_SECURE_NO_WARNINGS','/DE2E_HOST_RESOLVER_TEST','/DE2E4_AUDIO_ON=1']+['/I'+str(path) for path in includes]
 try:
  for name,source in [('runtime',overlay/'runtime.cpp'),('data',ROOT/'port/matrixgame/native/rangers_data_session.cpp')]:
   command=[str(compiler(env)),*flags,'/c',str(source),'/Fo'+str(BASE/(name+'.obj'))]
   p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;(BASE/('smoke-'+name+'.log')).write_text(log,encoding='utf-8')
   result['commands'].append({'label':name,'command':command,'exit':p.returncode});print(name,p.returncode,log[-9000:] if p.returncode else '',flush=True);assert not p.returncode,name
  result['status']='PASS; MSVC COMPILE SMOKE ONLY; NO RUNTIME OR WINDOWS REFERENCE CLAIM'
 finally:
  result['overlay_input_sha256']=sha(ROOT/'port/switch/build/e2e-game-perf515/runtime-overlay/windows.hpp');result['overlay_output_sha256']=sha(overlay/'windows.hpp')
  (BASE/'smoke-result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
