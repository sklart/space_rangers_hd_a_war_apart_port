"""Capture a native Windows failure without recompiling the original corpus."""
import json
import subprocess
import shutil
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha
from matrixgame_windows_host_env import environment
BASE=ROOT/'build/matrixgame/e2e579-hosted-windows-map'
def main():
 results=sorted(BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True);previous=read(results[0])
 env=environment(BASE);env['SDL_AUDIODRIVER']='dummy';env['SDL_VIDEODRIVER']='dummy';env['_NT_SYMBOL_PATH']=''
 out=BASE/'debug';out.mkdir(exist_ok=True);index=1
 while (out/f'run-{index}.json').exists():index+=1
 binary=out/f'hosted-map-{index}.exe';mapping=out/f'hosted-map-{index}.map';log=out/f'cdb-{index}.log'
 command=next(c['command'] for c in previous['commands'] if c['label']=='link')
 command=[('/Fe'+str(binary)) if s.startswith('/Fe') else s for s in command]+['/MAP:'+str(mapping)]
 linked=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');assert not linked.returncode,linked.stdout+linked.stderr
 link_command=command[:]
 for name in ('D3DX9_43.dll','D3DCompiler_43.dll'):
  source=BASE/name;target=out/name
  if not target.exists():shutil.copyfile(source,target)
  assert sha(source)==sha(target)
 # Real SDK DLLs remain in their workspace package; no system installation.
 env['PATH']=str(BASE)+';'+env['PATH']
 runtime=next(c['command'] for c in previous['commands'] if c['label']=='runtime');runtime[0]=str(binary)
 cdb=Path('C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe');assert cdb.is_file()
 command=[str(cdb),'-c','g; .exr -1; r; kb; dq @rsp L10; q','-logo',str(log),*runtime]
 debug=subprocess.run(command,cwd=BASE/'runtime',env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=90)
 (out/f'cdb-{index}-stdout.log').write_text(debug.stdout+debug.stderr,encoding='utf-8')
 (out/f'run-{index}.json').write_text(json.dumps({'scope':'DEBUG ONLY; NO RUNTIME PASS','link_command':link_command,'debug_command':command,'debug_exit':debug.returncode,'mapping_sha256':sha(mapping),'binary_sha256':sha(binary),'log':str(log)},indent=2)+'\n',encoding='utf-8')
 print((debug.stdout+debug.stderr)[-18000:],flush=True)
if __name__=='__main__':main()
