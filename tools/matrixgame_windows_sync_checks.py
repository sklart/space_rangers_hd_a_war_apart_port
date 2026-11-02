"""Validate generated native Windows lock storage against the installed SDK."""
import json
import subprocess
import time
from pathlib import Path
from e2e_runtime_overrides import prepare_runtime_overlay
from matrixgame_standard_nro_audit import ROOT,read,sha
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e577-rangers-windows'
def main():
 out=BASE/'sync'/('attempt-'+str(time.time_ns()));out.mkdir(parents=True)
 env=environment(out);overlay=BASE/'runtime-overlay'
 prepare_runtime_overlay(ROOT/'upstream/cpp/runtime',overlay,portable_windows_host=True)
 sources=[ROOT/'tests/probe_matrixgame_windows_sync.cpp',ROOT/'tests/probe_matrixgame_windows_sync_layout.cpp']
 flags=read(BASE/'result.json')['flags'];result={'status':'INCOMPLETE','commands':[],'source_sha256':{str(p.relative_to(ROOT)):sha(p) for p in sources},'overlay_sha256':sha(overlay/'sync.hpp')}
 def run(command,label):
  p=subprocess.run(list(map(str,command)),cwd=out,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr
  (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':list(map(str,command)),'exit':p.returncode});print(label,p.returncode,log,flush=True);assert not p.returncode;return log
 try:
  binary=out/'sync.exe'
  run([compiler(env),*flags,*sources,BASE/'objects/original/runtime.obj','/Fe'+str(binary),'/link','/INCREMENTAL:NO','oleaut32.lib','winmm.lib'],'compile-link')
  result['stdout']=run([binary],'runtime');assert 'cycles=4 recursive=PASS contended=PASS cleanup=PASS' in result['stdout']
  result['status']='PASS; SDK WINDOWS X64 NATIVE CRITICAL_SECTION LAYOUT AND EXECUTION'
 finally:
  (out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('result',out/'result.json',flush=True)
if __name__=='__main__':main()
