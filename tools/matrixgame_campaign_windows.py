"""Real original generation through the port owners, without save files or a Switch."""
import json,subprocess
from pathlib import Path
from matrixgame_campaign_overrides import apply
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e598-campaign'
def main():
 parent=BASE/'windows-asan';parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();env=environment(out);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy');env.pop('E2E511_HOST_GENERATION_SEED',None)
 prior_dir=ROOT/'build/matrixgame/e2e597-global-ui/windows-asan/4';prior=read(prior_dir/'result.json');assert prior['status'].startswith('PASS');check_hashes(prior['link_inputs_sha256']);check_hashes(prior['sources_sha256'])
 original=ROOT/'build/matrixgame/e2e577-rangers-windows/source/fGameSettings.cpp';assert original.is_file()
 fixed=out/'fGameSettings.cpp';fixed.write_text(apply(original.read_text(encoding='utf-8')),encoding='utf-8')
 prefix=(ROOT/'tests/probe_matrixgame_global_ui.cpp').read_text(encoding='utf-8');assert prefix.count('int main(int argc,char** argv){')==1
 probe=out/'campaign_probe.cpp';probe.write_text('#include "rangers_campaign_session.hpp"\n#include "e2e3_generation_state.hpp"\n#include "units/aRanger.hpp"\n#include "types/aMyFunction.hpp"\n'+prefix[:prefix.index('int main(int argc,char** argv){')]+(ROOT/'tests/matrixgame_campaign_probe.inc').read_text(encoding='utf-8'),encoding='utf-8')
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','battle_Gov_return':'NOT RUN','attempt':i,'generation_input':'ORIGINAL RANDOMIZE; NO SEED OVERRIDE','original_source_sha256':sha(original),'generated_source_sha256':sha(fixed)}
 def run(cmd,label,cwd=ROOT):
  cmd=list(map(str,cmd))
  try:
   p=subprocess.run(cmd,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=600);log=p.stdout+p.stderr;code=p.returncode
  except subprocess.TimeoutExpired as e:
   def decode(s):return s.decode('utf-8',errors='replace') if isinstance(s,bytes) else s or ''
   log=decode(e.stdout)+decode(e.stderr);code='TIMEOUT'
  path=out/(label+'.log');path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':cmd,'exit':code,'log':str(path.relative_to(ROOT))})
  print(label,code,log[-14000:] if code or label=='runtime' else '',flush=True);assert code==0,label;return log
 try:
  flags=prior['flags'];sources=[ROOT/'port/matrixgame/native/rangers_campaign_session.cpp',ROOT/'port/matrixgame/native/rangers_data_session.cpp',fixed,probe];objects=[]
  for source in sources:
   obj=out/(source.stem+'.obj');run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+source.stem);objects.append(obj)
  inputs=[Path(line.strip('"')) for line in (prior_dir/'link.rsp').read_text().splitlines() if Path(line.strip('"')).name not in ('probe_matrixgame_global_ui.obj','fGameSettings.obj','rangers_data_session.obj')]+objects
  rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+str(p)+'"' for p in inputs),encoding='utf-8');binary=out/'campaign.exe'
  command=next(c['command'] for c in prior['commands'] if c['label']=='link')[:];command=[('@'+str(rsp)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else s for s in command]
  command+=['/MAP:'+str(out/'campaign.map')];run(command,'link')
  env['PATH']=str(ROOT/'build/matrixgame/e2e590-completed-battle/fixed')+';'+env['PATH'];runtime=out/'runtime';runtime.mkdir()
  result['stdout']=run([binary,ROOT/'windows/Space Rangers HD A War Apart',runtime],'runtime',out)
  assert 'original_generation_complete=PASS stage=8' in result['stdout'] and 'original_world_worker_cleanup=PASS no_save_load=PASS victory_Gov_return=NOT_RUN' in result['stdout']
  check_hashes(prior['link_inputs_sha256']);check_hashes(prior['sources_sha256'])
  result.update(status='PASS; PARTIAL WINDOWS ASAN ORIGINAL FULL CAMPAIGN/PLAYER/WORKER/CLEANUP; NO BATTLE/GOV RETURN',flags=flags,sources_sha256={str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'port/matrixgame/native/rangers_campaign_session.hpp',ROOT/'tests/matrixgame_campaign_probe.inc',ROOT/'tools/matrixgame_campaign_overrides.py',ROOT/'tools/e2e_source_overrides.py',Path(__file__).resolve()]},link_inputs_sha256={str(p):sha(p) for p in inputs},binary_sha256=sha(binary))
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
