"""Extend the preserved real E608 scenario to two battles in the same process."""
import hashlib,json,subprocess
from pathlib import Path
from matrixgame_windows_host_env import environment
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'build/matrixgame/windows-reference-rebuild-20261123'
OLD=ROOT/'build/matrixgame/e2e608-ui-campaign/windows-difficulty3-economy-campaign0-winter1_1e-heavy-squad-caller-lease/1'
def sha(path):
 with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
 out=BASE/'gov-frun-two-link'
 if out.exists():raise RuntimeError('two-battle link output already exists')
 out.mkdir();env=environment(out);prior=BASE/'gov-frun-link-3';old=json.loads((prior/'result.json').read_text(encoding='utf8'));assert old['status'].startswith('PASS')
 result={'status':'INCOMPLETE','commands':[],'scenario':'two natural battles, one process and campaign, same window/device'}
 def run(label,cmd):
  cmd=list(map(str,cmd));p=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf8',errors='replace')
  (out/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf8');result['commands'].append({'label':label,'exit':p.returncode,'command':cmd});print(label,p.returncode,flush=True)
  if p.returncode:raise RuntimeError('see '+label+'.log')
 try:
  original=OLD/'campaign_frun.cpp';text=original.read_text(encoding='utf8')
  begin='   auto* player=aPlayer::GetPlayer();const auto player_id=player->Id;const auto planet_id=player->CurrentPlanet->Id;'
  end='   gov->OnClose();\n  }\n  GlobalsV::CurrentScreenId=previous_screen;'
  owner='   RangersCallerPresentation caller_presentation(window,device,[&]{return window.Attach(&platform3g::WindowProcedure);});\n'
  assert text.count(begin)==1 and text.count(end)==1 and text.count(owner)==1
  text=text.replace(owner,'',1)
  text=text.replace(begin,owner+'   for(int battle_index=0;battle_index<2;++battle_index){\n'
      '    campaign_build_commands=0;campaign_order_polls=0;campaign_design_selected=false;campaign_last_squad_time=-30000;\n'
      '    std::printf("GOV_FRUN battle_index=%d begin\\n",battle_index+1);\n'+begin,1)
  text=text.replace(end,'   gov->OnClose();\n   std::printf("GOV_FRUN battle_index=%d completed\\n",battle_index+1);\n   }\n  }\n  GlobalsV::CurrentScreenId=previous_screen;',1)
  source=out/'campaign_frun_two.cpp';source.write_text(text,encoding='utf8')
  (out/'hosted_map_build.hpp').write_text('inline constexpr char hosted_map_source_sha[]="'+sha(source)+'";\n',encoding='utf8')
  entry=next(x for x in old['commands'] if x['label']=='compile-campaign_frun');command=entry['command'][:]
  command.insert(1,'/I'+str(out));command[command.index('/c')+1]=str(source)
  obj=out/'campaign_frun_two.obj';command=[('/Fo'+str(obj)) if x.startswith('/Fo') else x for x in command];run('compile',command)
  old_obj=prior/'campaign_frun.obj';items=(prior/'link.rsp').read_text(encoding='utf8').splitlines()
  assert sum(line.strip('"')==str(old_obj) for line in items)==1
  rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+str(obj)+'"' if x.strip('"')==str(old_obj) else x for x in items),encoding='utf8')
  link=next(x for x in old['commands'] if x['label']=='link')['command'][:]
  binary=out/'gov-frun-two.exe';link=[('@'+str(rsp)) if x.startswith('@') else ('/Fe'+str(binary)) if x.startswith('/Fe') else x for x in link]
  run('link',link);result.update(status='PASS; two-battle Windows reference linked; runtime NOT RUN',source_sha256=sha(source),binary_sha256=sha(binary),baseline_source_sha256=sha(original))
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
if __name__=='__main__':main()
