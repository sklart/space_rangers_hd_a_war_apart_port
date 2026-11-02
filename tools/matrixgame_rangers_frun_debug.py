"""Preserve and diagnose an original FRun caller failure; no PASS claim."""
import argparse,json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha
from matrixgame_windows_host_env import environment
BASE=ROOT/'build/matrixgame/e2e593-rangers-caller-reset/frun-windows-asan'
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--base',type=Path);parser.add_argument('--map-only',action='store_true');args=parser.parse_args()
 if args.base:BASE=args.base.resolve()
 prior=read(max(BASE.glob('result*.json'),key=lambda p:p.stat().st_mtime_ns));out=BASE/'debug';out.mkdir(exist_ok=True)
 i=1
 while (out/f'run-{i}.json').exists():i+=1
 binary=out/f'frun-{i}.exe';mapping=out/f'frun-{i}.map';log=out/f'cdb-{i}.log'
 env=environment(out);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy');env['PATH']=str(ROOT/'build/matrixgame/e2e590-completed-battle/fixed')+';'+env['PATH'];env['_NT_SYMBOL_PATH']=''
 link=next(c['command'] for c in prior['commands'] if c['label']=='link');link=[('/Fe'+str(binary)) if v.startswith('/Fe') else v for v in link]+['/MAP:'+str(mapping)]
 p=subprocess.run(link,cwd=ROOT,env=env,capture_output=True,text=True,errors='replace');assert not p.returncode,p.stdout+p.stderr
 if args.map_only:
  (out/f'run-{i}.json').write_text(json.dumps({'status':'MAP ONLY; NO RUNTIME','command':link,'binary_sha256':sha(binary),'map_sha256':sha(mapping)},indent=2)+'\n',encoding='utf-8')
  print(mapping);raise SystemExit(0)
 runtime=next(c['command'] for c in prior['commands'] if c['label']=='runtime')[:];runtime[0]=str(binary)
 command=['C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe','-c','sxd eh; g; .exr -1; r; kb; u @rip-20 L20; q','-logo',str(log),*runtime]
 p=subprocess.run(command,cwd=BASE/'runtime',env=env,capture_output=True,text=True,errors='replace',timeout=90)
 (out/f'run-{i}.json').write_text(json.dumps({'status':'DEBUG ONLY','commands':[link,command],'exit':p.returncode,'binary_sha256':sha(binary),'map_sha256':sha(mapping)},indent=2)+'\n',encoding='utf-8')
 print((p.stdout+p.stderr)[-14000:])
