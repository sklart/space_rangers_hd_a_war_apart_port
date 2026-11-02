"""Real Init/Draw/FRun/Gov snapshot recovery after a typed native exception."""
import json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler
from matrixgame_campaign_exception_overrides import apply

def main():
 parent=ROOT/'build/matrixgame/e2e603-campaign-exception/windows';parent.mkdir(parents=True,exist_ok=True);index=1
 while (parent/str(index)).exists():index+=1
 out=parent/str(index);out.mkdir();env=environment(out);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='windows');env.pop('E2E511_HOST_GENERATION_SEED',None)
 baseline=ROOT/'build/matrixgame/e2e602-campaign-frun/windows-difficulty3-economy-campaign0-winter1_1e-heavy/2';gate=read(baseline/'result.json');assert gate['status'].startswith('PASS');check_hashes(gate['objects_sha256']);check_hashes(gate['reused_inputs_sha256'])
 source=(baseline/'campaign_frun.cpp').read_text(encoding='utf-8')
 a='struct Driver final:native_loop::Backend {';assert source.count(a)==1
 source=source.replace(a,'struct BattleProbeFailure:std::runtime_error {BattleProbeFailure():std::runtime_error("E603 typed battle cancellation") {}};\n'+a)
 a='if(pumps>=9){';assert source.count(a)==1
 source=source.replace(a,a+'std::puts("BATTLE_EXCEPTION original_Init_Draw_then_typed_throw=PASS");std::fflush(stdout);throw BattleProbeFailure();')
 a='   {native_loop::Scope loop(backend);gov->OnOpen();}'
 assert source.count(a)==1
 source=source.replace(a,'''   bool observed=false;
   try{native_loop::Scope loop(backend);gov->OnOpen();}
   catch(const BattleProbeFailure& error){observed=true;assert(std::string(error.what())=="E603 typed battle cancellation");}
   assert(observed && backend.draws>=8);
''')
 start=source.index('   assert(backend.natural_outcome==2 || backend.natural_outcome==3);')
 end=source.index('   gov->OnClose();\n  }',start)
 source=source[:start]+'''   assert(gov->PendingTransition==0 && backend.natural_outcome==0);
   player=aPlayer::GetPlayer();assert(player && player->Id==player_id && player->CurrentPlanet && player->CurrentPlanet->Id==planet_id);
   assert(campaign.Galaxy().CurrentTurn==turn && player->Money==money && player->PlanetBattles==battles && player->PlanetBattleHistory.length()==history);
   std::puts("BATTLE_EXCEPTION original_exception_type=PASS real_snapshot_restore=PASS no_reward_or_history=PASS pending_request_cleared=PASS");
'''+source[end:]
 probe=out/'campaign_exception.cpp';probe.write_text(source,encoding='utf-8')
 (out/'hosted_map_build.hpp').write_text('inline constexpr char hosted_map_source_sha[]="'+sha(probe)+'";\n',encoding='utf-8')
 main_source=out/'campaign_sdk_main.cpp';main_source.write_bytes((baseline/'campaign_sdk_main.cpp').read_bytes())
 sources=[probe,main_source]
 for name in ('Robot.cpp','fGov.cpp'):
  original=ROOT/'build/matrixgame/e2e577-rangers-windows/source'/name
  target=out/name;target.write_text(apply(name,original.read_text(encoding='utf-8')),encoding='utf-8');sources.append(target)
 flags=gate['flags'][:];flags=[('/I'+str(out)) if flag=='/I'+str(baseline) else flag for flag in flags]
 result={'status':'INCOMPLETE','scope':'real Windows D3D9 Init/Draw, typed native exception, original FRun/Gov recovery; no natural battle outcome claimed','commands':[],'flags':flags,'sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'tools/matrixgame_campaign_exception_overrides.py',Path(__file__).resolve()]},'Switch':'NOT RUN','new_NRO':False}
 def run(command,label,cwd=ROOT):
  command=list(map(str,command));logfile=out/(label+'.log')
  with logfile.open('w',encoding='utf-8') as stream:
   p=subprocess.run(command,cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=900)
  log=logfile.read_text(encoding='utf-8',errors='replace');result['commands'].append({'label':label,'command':command,'exit':p.returncode})
  if label=='runtime':result['stdout']=log
  print(label,p.returncode,log[-6000:] if p.returncode else '\n'.join(l for l in log.splitlines() if l.startswith(('BATTLE_EXCEPTION','RANGERS_CALLER','GOV_FRUN'))),flush=True)
  assert p.returncode==0,label
 try:
  objects=[]
  for source in sources:
   obj=out/(source.stem+'.obj');run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+source.stem);objects.append(obj)
  inputs=[Path(line.strip('"')) for line in (baseline/'link.rsp').read_text().splitlines() if Path(line.strip('"')).name not in ('campaign_frun.obj','campaign_sdk_main.obj','Robot.obj','fGov.obj')]
  hashes={str(p):sha(p) for p in inputs};rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+str(p)+'"' for p in [*inputs,*objects]),encoding='utf-8');binary=out/'campaign-exception.exe'
  command=next(c['command'] for c in gate['commands'] if c['label']=='link')[:]
  command=[('@'+str(rsp)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else ('/MAP:'+str(out/'campaign-exception.map')) if s.startswith('/MAP:') else s for s in command];run(command,'link')
  result.update(reused_inputs_sha256=hashes,objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects},binary_sha256=sha(binary))
  runtime=out/'runtime';runtime.mkdir();output=runtime/'output';output.mkdir();env['PATH']=str(ROOT/'build/matrixgame/e2e590-completed-battle/fixed')+';'+env['PATH']
  run([binary,output,ROOT/'windows/Space Rangers HD A War Apart',ROOT/'build/matrixgame/e2e578-hosted-windows-native/prefix/native-input'],'runtime',runtime)
  for marker in ('original_exception_type=PASS real_snapshot_restore=PASS no_reward_or_history=PASS','configured_before_battle_equals_after_FRun=PASS','original_campaign_UI_script_host_cleanup=PASS'):assert marker in result['stdout'],marker
  check_hashes(hashes);check_hashes(result['sources_sha256']);result['status']='PASS REAL WINDOWS INIT/DRAW/TYPED EXCEPTION/CALLER RESTORE/MEMORY SNAPSHOT/CLEANUP; PARTIAL ASAN'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
