"""Run genuine campaign and normal landing orders on the Windows reference."""
import argparse, json, subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT, read, sha, check_hashes
from matrixgame_windows_host_env import environment, compiler

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--government',action='store_true');parser.add_argument('--snapshot',action='store_true');args=parser.parse_args();snapshot=args.snapshot;government=args.government or snapshot
 parent=ROOT/'build/matrixgame/e2e600-campaign-navigation'/('snapshot-windows' if snapshot else 'government-windows' if government else 'windows');parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();old=ROOT/'build/matrixgame/e2e599-campaign-cleanup/windows/1';prior=read(old/'result.json');assert prior['status'].startswith('PASS')
 check_hashes(prior['objects_sha256']);check_hashes(prior['reused_objects_sha256']);check_hashes(prior['sources_sha256'])
 template=ROOT/'build/matrixgame/e2e598-campaign/windows-asan/5/campaign_probe.cpp';text=template.read_text(encoding='utf-8')
 inc=ROOT/'tests/matrixgame_campaign_navigation.inc';assert text.count('int main(int argc,char** argv){')==1
 extra=ROOT/'tests/matrixgame_campaign_government.inc';snapshot_inc=ROOT/'tests/matrixgame_campaign_snapshot.inc';prefix=inc.read_text(encoding='utf-8')+(extra.read_text(encoding='utf-8') if government else '')+(snapshot_inc.read_text(encoding='utf-8') if snapshot else '')
 text=text.replace('int main(int argc,char** argv){',prefix+'\nint main(int argc,char** argv){')
 anchor='   Reject([]{m::RangersCampaignSession duplicate;});';assert text.count(anchor)==1;text=text.replace(anchor,'   ExerciseOriginalLanding();\n'+('   ExerciseOriginalGovernment();\n' if government else '')+('   ExerciseOriginalSnapshot(campaign);player=aPlayer::GetPlayer();\n' if snapshot else '')+anchor)
 if snapshot:
  for member in ('Stars','Planets','Rangers'):text=text.replace('pas::list_count(world.'+member+')','pas::list_count(aGalaxy::Galaxy->'+member+')')
  text=text.replace('world.CurrentTurn','aGalaxy::Galaxy->CurrentTurn')
 source=out/'navigation_probe.cpp';source.write_text(text,encoding='utf-8');env=environment(out);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='dummy');env.pop('E2E511_HOST_GENERATION_SEED',None)
 result={'status':'INCOMPLETE','commands':[],'battle_Gov_return':'NOT RUN','Switch':'NOT RUN','sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [template,inc,source,Path(__file__).resolve(),*([extra] if government else []),*([snapshot_inc] if snapshot else [])]},'prior_result_sha256':sha(old/'result.json')}
 def run(command,label,cwd=ROOT):
  command=list(map(str,command));p=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=600);log=p.stdout+p.stderr
  (out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-12000:] if p.returncode or label=='runtime' else '',flush=True)
  if label=='runtime':result['stdout']=log
  assert p.returncode==0,label
 try:
  obj=out/'navigation_probe.obj';run([compiler(env),*prior['flags'],'/c',source,'/Fo'+str(obj)],'compile')
  exclude={'campaign_probe.obj'}|({'rangers_campaign_session.obj','aGalaxy.obj','aShip.obj'} if snapshot else set())
  inputs=[Path(line.strip('"')) for line in (old/'link.rsp').read_text().splitlines() if Path(line.strip('"')).name not in exclude];hashes={str(p):sha(p) for p in inputs};objects=[obj]
  if snapshot:
   for name in ('rangers_campaign_session','rangers_achievement_session'):
    cpp=ROOT/'port/matrixgame/native'/(name+'.cpp');compiled=out/(name+'.obj');run([compiler(env),*prior['flags'],'/c',cpp,'/Fo'+str(compiled)],'compile-'+name);objects.append(compiled);result['sources_sha256'][str(cpp.relative_to(ROOT))]=sha(cpp)
   for name in ('rangers_campaign_session.hpp','rangers_achievement_session.hpp'):
    header=ROOT/'port/matrixgame/native'/name;result['sources_sha256'][str(header.relative_to(ROOT))]=sha(header)
   from matrixgame_campaign_cleanup_overrides import apply as cleanup
   from matrixgame_campaign_snapshot_overrides import apply as snapshot_cleanup
   for name in ('aGalaxy.cpp','aShip.cpp'):
    original=ROOT/'build/matrixgame/e2e577-rangers-windows/source'/name;cpp=out/name;cpp.write_text(snapshot_cleanup(name,cleanup(name,original.read_text(encoding='utf-8'))),encoding='utf-8');compiled=out/(cpp.stem+'.obj');run([compiler(env),*prior['flags'],'/c',cpp,'/Fo'+str(compiled)],'compile-'+cpp.stem);objects.append(compiled)
    for p in (original,cpp):result['sources_sha256'][str(p.relative_to(ROOT))]=sha(p)
  rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+str(p)+'"' for p in [*inputs,*objects]),encoding='utf-8');binary=out/'navigation.exe'
  command=next(c['command'] for c in prior['commands'] if c['label']=='link')[:];command=[('@'+str(rsp)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else ('/MAP:'+str(out/'navigation.map')) if s.startswith('/MAP:') else s for s in command];run(command,'link')
  result.update(binary_sha256=sha(binary),reused_inputs_sha256=hashes,objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects});runtime=out/'runtime';runtime.mkdir();env['PATH']=str(ROOT/'build/matrixgame/e2e590-completed-battle/fixed')+';'+env['PATH']
  run([binary,ROOT/'windows/Space Rangers HD A War Apart',runtime],'runtime',out)
  assert 'NAVIGATION original_landing=PASS' in result['stdout'];check_hashes(hashes);check_hashes(result['sources_sha256'])
  if government:assert 'GOVERNMENT original_OnOpen=PASS' in result['stdout'] and 'GOVERNMENT original_OnClose=PASS' in result['stdout']
  if snapshot:assert 'SNAPSHOT original_memory_roundtrip=PASS' in result['stdout']
  result['status']='PASS; PARTIAL WINDOWS ASAN ORIGINAL CAMPAIGN TAKEOFF/TRAVEL/LANDING'+('/GOV ONOPEN' if government else '')+('/MEMORY SNAPSHOT' if snapshot else '')+'; NO BATTLE/GOV RETURN'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
