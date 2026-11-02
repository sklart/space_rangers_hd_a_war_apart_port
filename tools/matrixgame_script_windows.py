"""Original script VM/events/path/worker lifecycle on Windows, without SDK window or battle."""
import json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e595-script-session/windows-asan'
def main():
 BASE.mkdir(parents=True,exist_ok=True);env=environment(BASE);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy')
 prior=read(ROOT/'build/matrixgame/e2e594-government-session/windows-asan-depth/result.json');assert prior['status'].startswith('PASS');check_hashes(prior['link_inputs_sha256'])
 result={'status':'INCOMPLETE','commands':[],'Galaxy':'NOT RUN','Switch':'NOT RUN'}
 sources=[ROOT/'port/matrixgame/native/rangers_script_session.cpp',ROOT/'tests/probe_matrixgame_script_session.cpp']
 def run(cmd,label):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=120);log=p.stdout+p.stderr;path=BASE/(label+'.log');i=1
  while path.exists():path=BASE/f'{label}-{i}.log';i+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-6000:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode,label;return log
 try:
  flags=prior['flags'];objects=[]
  for source in sources:
   obj=BASE/(source.stem+'.obj');run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+source.stem);objects.append(obj)
  old=ROOT/'build/matrixgame/e2e594-government-session/windows-asan-depth'
  inputs=[Path(line.strip('"')) for line in (old/'link.rsp').read_text().splitlines()];inputs=[p for p in inputs if p.name not in ('frun_reference.obj','frun_sdk_main.obj','rangers_government_session.obj')]+objects
  response=BASE/'link.rsp';response.write_text('\n'.join('"'+str(p)+'"' for p in inputs),encoding='utf-8');binary=BASE/'script-session.exe'
  command=next(c['command'] for c in prior['commands'] if c['label']=='link');command=[('@'+str(response)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else s for s in command];run(command,'link')
  env['PATH']=str(ROOT/'build/matrixgame/e2e590-completed-battle/fixed')+';'+env['PATH'];runtime=BASE/'runtime';runtime.mkdir(exist_ok=True)
  result['stdout']=run([binary,ROOT/'windows/Space Rangers HD A War Apart',runtime],'runtime')
  assert result['stdout'].count('original_VM_abs_assignment=42 path_allocate_recycle=PASS auto_manual_events=PASS real_worker_standby=PASS Galaxy=NOT_CREATED')==2
  assert 'original_Finalize=PASS events_worker_closed=PASS pool_heads_counts_callback_restored=PASS foreign_owner=REJECTED no_save_load=PASS NO_WORLD_GENERATION' in result['stdout']
  result.update(status='PASS; PARTIAL WINDOWS ASAN REAL SCRIPT VM/EVENTS/WORKER/PATH LIFECYCLE; NO GALAXY',flags=flags,sources_sha256={str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'port/matrixgame/native/rangers_script_session.hpp',Path(__file__).resolve()]},link_inputs_sha256={str(p):sha(p) for p in inputs},binary_sha256=sha(binary))
 finally:
  path=BASE/'result.json';i=1
  while path.exists():path=BASE/f'result-{i}.json';i+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
