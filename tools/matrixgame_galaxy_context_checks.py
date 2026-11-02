"""Inspect real standalone host preconditions; never call uninitialized galaxy UI."""
import json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e590-completed-battle/galaxy-context-fixed'
def main():
 BASE.mkdir(parents=True,exist_ok=True);env=environment(BASE);env['SDL_AUDIODRIVER']='dummy';env['SDL_VIDEODRIVER']='dummy'
 fixed=ROOT/'build/matrixgame/e2e590-completed-battle/fixed';prior=read(fixed/'result-5.json');assert prior['status'].startswith('PASS');check_hashes(prior['link_inputs_sha256'])
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN'}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace');log=p.stdout+p.stderr;path=BASE/(label+'.log');assert not path.exists();path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-3500:] if p.returncode or label=='runtime' else '',flush=True);assert not p.returncode;return log
 try:
  source=ROOT/'tests/matrixgame_galaxy_context.cpp';obj=BASE/'context.obj';run([compiler(env),*prior['flags'],'/c',source,'/Fo'+str(obj)],'compile')
  inputs=[Path(line.strip('"')) for line in (fixed/'link.rsp').read_text().splitlines()]
  inputs=[p for p in inputs if p.name=='observed_sound_legacy.obj' or not (p.parent==fixed and p.suffix in ('.obj','.res'))];inputs.insert(0,obj)
  response=BASE/'link.rsp';response.write_text('\n'.join('"'+str(p)+'"' for p in inputs),encoding='utf-8')
  command=next(c['command'] for c in prior['commands'] if c['label']=='link');binary=BASE/'context.exe'
  command=[('@'+str(response)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else s for s in command];run(command,'link')
  user=BASE/'private-user';user.mkdir(exist_ok=False);result['stdout']=run([binary,ROOT/'windows/Space Rangers HD A War Apart',user],'runtime')
  assert 'standalone_host_is_not_galaxy_session=CONFIRMED cleanup=PASS no_save_load=PASS' in result['stdout']
  result.update(status='PASS; STANDALONE HOST HAS NO GALAXY/PLAYER/GOVERNMENT SCREEN/RUN INTERFACE; FULL GALAXY RETURN NOT EXECUTED',sources_sha256={str(p.relative_to(ROOT)):sha(p) for p in (source,Path(__file__).resolve())},link_inputs_sha256={str(p):sha(p) for p in inputs},binary_sha256=sha(binary),full_galaxy_return='NOT RUN')
 finally:
  with (BASE/'result.json').open('x',encoding='utf-8') as stream:json.dump(result,stream,indent=2);stream.write('\n')
if __name__=='__main__':main()
