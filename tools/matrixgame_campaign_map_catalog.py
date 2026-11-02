"""Read the real original Gov map catalog without another campaign/warmup or battle."""
import json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler

def main():
 base=ROOT/'build/matrixgame/e2e602-campaign-frun/map-catalog';base.mkdir(parents=True,exist_ok=True);i=1
 while (base/str(i)).exists():i+=1
 out=base/str(i);out.mkdir();env=environment(out);old=ROOT/'build/matrixgame/e2e600-campaign-navigation/snapshot-windows/4';prior=read(old/'result.json');assert prior['status'].startswith('PASS');check_hashes(prior['reused_inputs_sha256']);check_hashes(prior['objects_sha256'])
 source=out/'catalog.cpp';source.write_text('''#include "rangers_host_session.hpp"
#include "ec_file_adapter.hpp"
#include "runtime_support.hpp"
#include "units/Globals.hpp"
#include "units/GR_Main.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char** argv){
 assert(argc==3);srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 {srhd_awa::matrixgame::RangersHostOptions options;options.sound=false;options.music=false;srhd_awa::matrixgame::RangersHostSession host(options);
 assert(!Globals::RobotMapDefinitions.length());struct Restore{~Restore(){Globals::RobotMapDefinitions=nullptr;}} restore;
 Globals::InitializeRobotMapDefinitions();assert(Globals::RobotMapDefinitions.length()>0);
 for(int i=0;i<Globals::RobotMapDefinitions.length();++i){const auto& m=Globals::RobotMapDefinitions[i];const auto path=static_cast<pas::AnsiString>(m.Map);
 std::printf("MAP\\t%d\\t%d\\t%d\\t%d\\t%d\\t%.*s\\n",m.Id,m.MinWins,m.MaxWins,m.Group,m.ReinforcementsDisabled,int(path.length()),reinterpret_cast<const char*>(path.data()));}
 }assert(!Globals::RobotMapDefinitions.length() && !GR_Main::MainDataConfig);std::puts("CATALOG original_definitions_cleanup=PASS no_campaign_or_save=PASS");
}
''',encoding='utf-8')
 result={'status':'INCOMPLETE','commands':[],'scope':'ORIGINAL DEFINITION READ ONLY; NO CAMPAIGN/WARMUP/BATTLE/SAVE','sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in (source,Path(__file__).resolve())}}
 def run(command,label):
  command=list(map(str,command));p=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180);log=p.stdout+p.stderr;(out/(label+'.log')).write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode});print(label,p.returncode,log[-12000:] if p.returncode or label=='runtime' else '',flush=True);assert p.returncode==0,label;return log
 try:
  flags=read(ROOT/'build/matrixgame/e2e599-campaign-cleanup/windows/1/result.json')['flags'];obj=out/'catalog.obj';run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile')
  inputs=[Path(line.strip('"')) for line in (old/'link.rsp').read_text().splitlines() if Path(line.strip('"')).name!='navigation_probe.obj'];hashes={str(p):sha(p) for p in inputs};rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+str(p)+'"' for p in [*inputs,obj]),encoding='utf-8');binary=out/'catalog.exe'
  command=next(c['command'] for c in prior['commands'] if c['label']=='link')[:];command=[('@'+str(rsp)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else ('/MAP:'+str(out/'catalog.map')) if s.startswith('/MAP:') else s for s in command];run(command,'link')
  user=out/'user';user.mkdir();log=run([binary,ROOT/'windows/Space Rangers HD A War Apart',user],'runtime');assert 'original_definitions_cleanup=PASS' in log;check_hashes(hashes)
  result.update(status='PASS; REAL GOV MAP CATALOG; NO CAMPAIGN OR SAVE',stdout=log,binary_sha256=sha(binary),object_sha256=sha(obj),reused_inputs_sha256=hashes)
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
