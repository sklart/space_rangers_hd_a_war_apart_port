"""Real Gov/FRun/MatrixGame/result on the Windows HAL; preserve all previous fixtures."""
import argparse,json,re,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler
from matrixgame_campaign_cleanup_overrides import apply as cleanup
from matrixgame_campaign_snapshot_overrides import apply as snapshot_cleanup
from matrixgame_campaign_revolution_overrides import apply as revolution
from matrixgame_campaign_exception_overrides import apply as battle_exception
from matrixgame_campaign_caller_overrides import apply as caller_restore

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--ui-context',action='store_true');parser.add_argument('--caller-lease',action='store_true');parser.add_argument('--formation',choices=('individual','squad'),default='individual');parser.add_argument('--design',choices=('stock','heavy'),default='stock');parser.add_argument('--difficulty',type=int,choices=(1,2,3),default=1);parser.add_argument('--strategy',choices=('attack','economy'),default='attack');parser.add_argument('--campaign-difficulty',type=int,choices=(0,1,2),default=2);parser.add_argument('--map',default='desert1_3e.cmap');args=parser.parse_args();difficulty=args.difficulty
 assert re.fullmatch(r'[A-Za-z0-9_ -]+\.cmap',args.map),args.map
 assert args.design=='stock' or args.strategy=='economy','heavy design requires real economy controller'
 assert args.formation=='individual' or args.strategy=='economy','squad requires real economy controller'
 parent=ROOT/'build/matrixgame/e2e602-campaign-frun'/(('windows' if difficulty==1 else 'windows-difficulty'+str(difficulty))+('-economy' if args.strategy=='economy' else '')+('-campaign'+str(args.campaign_difficulty) if args.campaign_difficulty!=2 else '')+('-'+Path(args.map).stem.lower() if args.map.lower()!='desert1_3e.cmap' else ''));parent=parent.with_name(parent.name+('-heavy' if args.design=='heavy' else ''));parent=parent.with_name(parent.name+('-squad' if args.formation=='squad' else ''));parent=parent.with_name(parent.name+('-caller-lease' if args.caller_lease else ''));parent=(ROOT/'build/matrixgame/e2e608-ui-campaign'/parent.name) if args.ui_context else parent;parent.mkdir(parents=True,exist_ok=True);i=1
 while (parent/str(i)).exists():i+=1
 out=parent/str(i);out.mkdir();env=environment(out);env.update(SDL_AUDIODRIVER='dummy',SDL_VIDEODRIVER='windows');env.pop('E2E511_HOST_GENERATION_SEED',None)
 old=ROOT/'build/matrixgame/e2e593-rangers-caller-reset/frun-windows-asan-gamma';prior=read(old/'result.json');assert prior['status'].startswith('PASS');check_hashes(prior['link_inputs_sha256'])
 world=ROOT/'build/matrixgame/e2e600-campaign-navigation/snapshot-windows/4';reference=read(world/'result.json');assert reference['status'].startswith('PASS');check_hashes(reference['objects_sha256']);check_hashes(reference['reused_inputs_sha256']);check_hashes(reference['sources_sha256'])
 driver=(old/'frun_reference.cpp').read_text(encoding='utf-8');driver=driver[:driver.index('// Appended to the preserved E592 real mission driver.')]
 commands=ROOT/'tests/matrixgame_campaign_player_commands.inc'
 if args.strategy=='economy':
  anchor='using namespace srhd_awa::matrixgame;';assert driver.count(anchor)==1;driver=driver.replace(anchor,'#include "MatrixObjectBuilding.hpp"\n#include "Interface/CInterface.h"\n#include "Interface/CCounter.h"\n#include "Logic/MatrixAIGroup.h"\n'+anchor)
  anchor='struct Driver final:native_loop::Backend {';assert driver.count(anchor)==1;driver=driver.replace(anchor,commands.read_text(encoding='utf-8')+'\n'+anchor)
  anchor='g_FormCur->Takt(25);++extra_ticks;';assert driver.count(anchor)==1;driver=driver.replace(anchor,'if(extra_ticks%200==0)CampaignPlayerCommands();'+anchor)
  anchor='if(pumps==9){Issue();Inventory();}';assert driver.count(anchor)==1;driver=driver.replace(anchor,'if(pumps==9){CampaignPlayerCommands();Inventory();}')
 helper=(ROOT/'tests/probe_matrixgame_global_ui.cpp').read_text(encoding='utf-8');helper=helper[:helper.index('int main(int argc,char** argv){')]
 # Borrow graphics from the real renderer_platform owner instead of creating
 # a second pixel/scratch-only context that cannot draw Gov loading progress.
 for pattern in (r'^ Owner<GR_GraphBuf::TPixelFormatGR> pixel.*\n',r'^ Owner<GR_GraphBuf::TGraphBufGR> scratch.*\n',r'^  pixel->RedMask=.*\n'):
  helper,count=re.subn(pattern,'',helper,flags=re.MULTILINE);assert count==1,pattern
 for before,after in (('!GR_Main::CurrentPixelFormat && !GR_Main::RenderScratchBuffer && ',''),('scratch->AllocateNative(size,size);GR_Main::RenderScratchBuffer=scratch.get();',''),('GR_Main::RenderScratchBuffer=nullptr;GR_Main::CurrentPixelFormat=nullptr;','')):
  assert helper.count(before)==1,before;helper=helper.replace(before,after)
 inc=ROOT/'tests/matrixgame_campaign_frun_reference.inc';scenario=inc.read_text(encoding='utf-8');assert scenario.count('u"desert1_3e.cmap"')==1;scenario=scenario.replace('u"desert1_3e.cmap"','u'+json.dumps(args.map.lower()))
 if args.ui_context:
  # Use the production owner, including real palette/cursors/depth and rollback.
  # Keep only helper includes and emptiness checks; the old Caller is not used.
  start=helper.index('struct Caller {');end=helper.index('\n};',start)+len('\n};');helper=helper[:start]+helper[end:]
  start=scenario.index('  m::RangersScriptSession scripts;');end=scenario.index('  GR_Main::PresentationWidth=',start)
  scenario=scenario[:start]+'  m::RangersUiContext ui_context;\n  m::RangersScriptSession scripts;\n  std::puts("GOV_FRUN production_UI_context=ACTIVE original_CFG_TCC_canvas_palette_cursors=PASS");\n'+scenario[end:]
  helper='#include "rangers_ui_context.hpp"\n'+helper
 probe=out/'campaign_frun.cpp';probe.write_text(driver+'\n#include "rangers_campaign_session.hpp"\n'+helper+'\n'+(ROOT/'tests/matrixgame_campaign_navigation.inc').read_text(encoding='utf-8')+'\n'+scenario,encoding='utf-8')
 main=(old/'frun_sdk_main.cpp').read_text(encoding='utf-8')
 for before,after in (('RECT client{0,0,800,600}','RECT client{0,0,1024,768}'),('restored.right-restored.left<800 || restored.bottom-restored.top<600','restored.right-restored.left<1024 || restored.bottom-restored.top<768'),('description.Width!=800 || description.Height!=600 || restored_view.Width!=800 || restored_view.Height!=600','description.Width!=1024 || description.Height!=768 || restored_view.Width!=1024 || restored_view.Height!=768')):
  assert main.count(before)==1,before;main=main.replace(before,after)
 if args.caller_lease:
  anchor='pp.BackBufferWidth=640;pp.BackBufferHeight=480;';assert main.count(anchor)==1;main=main.replace(anchor,'pp.BackBufferWidth=1024;pp.BackBufferHeight=768;')
 main_source=out/'campaign_sdk_main.cpp';main_source.write_text(main,encoding='utf-8')
 original=ROOT/'build/matrixgame/e2e577-rangers-windows/source/aPlanet.cpp';planet=out/'aPlanet.cpp';planet.write_text(revolution('aPlanet.cpp',snapshot_cleanup('aPlanet.cpp',cleanup('aPlanet.cpp',original.read_text(encoding='utf-8')))),encoding='utf-8')
 if args.caller_lease:
  expanded=probe.read_text(encoding='utf-8');anchor='   Driver backend;';assert expanded.count(anchor)==1
  expanded='#include "rangers_caller_presentation.hpp"\n'+expanded.replace(anchor,'   RangersCallerPresentation caller_presentation(window,device,[&]{return window.Attach(&platform3g::WindowProcedure);});\n'+anchor)
  anchor='   VerifyCallerGammaAfterBattle();';assert expanded.count(anchor)==1
  expanded=expanded.replace(anchor,'   assert(caller_presentation.WindowOwned() && !caller_presentation.BattleActive());std::puts("CALLER_LEASE natural_return_window_owned=PASS battle_lease_released=PASS");\n'+anchor)
  probe.write_text(expanded,encoding='utf-8')
 flags=read(ROOT/'build/matrixgame/e2e599-campaign-cleanup/windows/1/result.json')['flags']+['/I'+str(out),'/I'+str(ROOT/'tests'),'/DMATRIXGAME_CAMPAIGN_SQUAD='+str(int(args.formation=='squad')),'/DMATRIXGAME_CAMPAIGN_HEAVY_DESIGN='+str(int(args.design=='heavy')),'/DMATRIXGAME_GOV_DIFFICULTY='+str(difficulty),'/DMATRIXGAME_CAMPAIGN_DIFFICULTY='+str(args.campaign_difficulty)]
 (out/'hosted_map_build.hpp').write_text('inline constexpr char hosted_map_source_sha[]="'+sha(probe)+'";\n',encoding='utf-8')
 sources=[probe,main_source,planet]
 for name in ('Robot.cpp','fGov.cpp'):
  target=out/name;target.write_text(caller_restore(name,battle_exception(name,(ROOT/'build/matrixgame/e2e577-rangers-windows/source'/name).read_text(encoding='utf-8'))),encoding='utf-8');sources.append(target)
 if args.caller_lease:sources.append(ROOT/'port/matrixgame/native-engine/rangers_caller_presentation.cpp')
 if args.ui_context:sources.extend([ROOT/'port/matrixgame/native/rangers_ui_context.cpp',ROOT/'port/switch/platform/renderer_platform.cpp'])
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','entry_boundary':'ORIGINAL GOV ONOPEN; UI AUTO-SAVE HANDLER NOT RUN','difficulty':difficulty,'campaign_difficulty':args.campaign_difficulty,'strategy':args.strategy,'design':args.design,'formation':args.formation,'map':args.map,'native_exception_restore':True,'caller_lease':args.caller_lease,'production_ui_context':args.ui_context,'natural_victory':'NOT PROVEN','sources_sha256':{str(p.relative_to(ROOT)):sha(p) for p in [*sources,inc,original,ROOT/'tests/probe_matrixgame_global_ui.cpp',ROOT/'tests/matrixgame_campaign_navigation.inc',ROOT/'tools/matrixgame_campaign_revolution_overrides.py',ROOT/'tools/matrixgame_campaign_exception_overrides.py',ROOT/'tools/matrixgame_campaign_caller_overrides.py',ROOT/'port/switch/platform/planet_battle_presentation.hpp',ROOT/'port/matrixgame/native-engine/rangers_caller_presentation.hpp',Path(__file__).resolve(),*([commands] if args.strategy=='economy' else [])]},'flags':flags}
 def run(command,label,cwd=ROOT,timeout=900):
  command=list(map(str,command))
  logfile=out/(label+'.log')
  try:
   # Keep the actual progress durable and inspectable while a battle is live.
   with logfile.open('w',encoding='utf-8') as stream:
    p=subprocess.run(command,cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=timeout)
   code=p.returncode
  except subprocess.TimeoutExpired:code='TIMEOUT'
  log=logfile.read_text(encoding='utf-8',errors='replace');result['commands'].append({'label':label,'command':command,'exit':code});print(label,code,log[-13000:] if code else '\n'.join(l for l in log.splitlines() if l.startswith(('GOV_FRUN','NAVIGATION','COMPLETION_NATURAL','RANGERS_CALLER'))),flush=True)
  if label=='runtime':result['stdout']=log
  assert code==0,label
 try:
  objects=[]
  for source in sources:
   obj=out/(source.stem+'.obj');run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+source.stem);objects.append(obj)
  inputs=[Path(line.strip('"')) for line in (world/'link.rsp').read_text().splitlines() if Path(line.strip('"')).name not in ('navigation_probe.obj','aPlanet.obj','Robot.obj','fGov.obj',*(['renderer_platform.obj'] if args.ui_context else []))];assert all(p.name not in ('frun_reference.obj','frun_sdk_main.obj') for p in inputs)
  hashes={str(p):sha(p) for p in inputs};rsp=out/'link.rsp';rsp.write_text('\n'.join('"'+str(p)+'"' for p in [*inputs,*objects]),encoding='utf-8');binary=out/'campaign-frun.exe'
  command=next(c['command'] for c in prior['commands'] if c['label']=='link')[:];command=[('@'+str(rsp)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else s for s in command];command+=['/MAP:'+str(out/'campaign-frun.map')];run(command,'link')
  result.update(reused_inputs_sha256=hashes,objects_sha256={str(p.relative_to(ROOT)):sha(p) for p in objects},binary_sha256=sha(binary));runtime=out/'runtime';runtime.mkdir();output=runtime/'output';output.mkdir();env['PATH']=str(ROOT/'build/matrixgame/e2e590-completed-battle/fixed')+';'+env['PATH']
  run([binary,output,ROOT/'windows/Space Rangers HD A War Apart',ROOT/'build/matrixgame/e2e578-hosted-windows-native/prefix/native-input'],'runtime',runtime)
  assert 'original_Gov_history=PASS actual_statistics=PASS' in result['stdout'] and 'original_campaign_UI_script_host_cleanup=PASS' in result['stdout'];check_hashes(hashes);check_hashes(result['sources_sha256'])
  if args.ui_context:assert 'production_UI_context=ACTIVE original_CFG_TCC_canvas_palette_cursors=PASS' in result['stdout']
  result['status']='PASS; PARTIAL WINDOWS ASAN ORIGINAL CAMPAIGN/GOV/FRUN/NATURAL OUTCOME/HISTORY/RETURN; UI AUTOSAVE HANDLER NOT RUN'
  result['natural_victory']='PASS' if 'GOV_FRUN actual_outcome=3 ' in result['stdout'] else 'NOT PROVEN'
 finally:(out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
