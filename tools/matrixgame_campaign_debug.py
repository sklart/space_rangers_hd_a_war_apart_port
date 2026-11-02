"""Bounded debugger run of an existing failed campaign binary; never a PASS gate."""
import argparse,json,subprocess
from matrixgame_standard_nro_audit import ROOT,read,sha
from matrixgame_windows_host_env import environment
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('attempt',type=int);args=parser.parse_args()
 base=ROOT/f'build/matrixgame/e2e598-campaign/windows-asan/{args.attempt}';prior=read(base/'result.json');assert prior['status']=='INCOMPLETE'
 out=base/'debug';out.mkdir(exist_ok=True);i=1
 while (out/f'run-{i}.json').exists():i+=1
 env=environment(out);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy');env['PATH']=str(ROOT/'build/matrixgame/e2e590-completed-battle/fixed')+';'+env['PATH'];env['_NT_SYMBOL_PATH']=''
 runtime=next(c['command'] for c in prior['commands'] if c['label']=='runtime')
 command=['C:/Program Files (x86)/Windows Kits/10/Debuggers/x64/cdb.exe','-c','sxd eh; sxd av; sxd 0xe0736170; sxe 0xc0000409; g; .exr -1; r; kb; u @rip-20 L20; q','-logo',str(out/f'cdb-{i}.log'),*runtime]
 p=subprocess.run(command,cwd=base,env=env,capture_output=True,text=True,errors='replace',timeout=120)
 (out/f'run-{i}.json').write_text(json.dumps({'status':'DEBUG ONLY','command':command,'exit':p.returncode,'binary_sha256':sha(base/'campaign.exe'),'map_sha256':sha(base/'campaign.map')},indent=2)+'\n',encoding='utf-8')
 print((p.stdout+p.stderr)[-18000:])
