"""Execute the real typed Rangers Run entry through complete original mission loss."""
import json,subprocess
from pathlib import Path
from matrixgame_standard_nro_audit import ROOT,read,sha,check_hashes
from matrixgame_windows_host_env import environment,compiler
BASE=ROOT/'build/matrixgame/e2e591-native-rangers-run/windows-asan'
def main():
 BASE.mkdir(parents=True,exist_ok=True);env=environment(BASE);env['SDL_AUDIODRIVER']='dummy';env['SDL_VIDEODRIVER']='dummy'
 fixed=ROOT/'build/matrixgame/e2e590-completed-battle/fixed';prior=read(fixed/'result-5.json');assert prior['status'].startswith('PASS');check_hashes(prior['link_inputs_sha256'])
 text=(fixed/'completed_loop_legacy_audio.cpp').read_text(encoding='utf-8')
 text=text[:text.index('int RunOriginalWindowsGame(')]
 text=text.replace('#include "init_runtime.hpp"','#include "init_runtime.hpp"\n#include "native_rangers_run.hpp"',1)
 text=text.replace(' unsigned extra_ticks{},issued_groups{};int natural_outcome{};',' unsigned extra_ticks{},issued_groups{},warming{};int natural_outcome{};SRobotGameState expected{};',1)
 text=text.replace('  Key(13);assert(g_ExitState==natural_outcome && (g_Flags&GFLAG_EXITLOOP));','  CGame::SaveResult(&expected);Key(13);assert(g_ExitState==natural_outcome && (g_Flags&GFLAG_EXITLOOP));',1)
 text=text.replace('  ++pumps;','  if(g_MatrixMap->m_Flags&MMFLAG_TRANSITION){events.FocusChanged(true);++warming;assert(warming<=200);return;}\n  ++pumps;',1)
 text=text.replace('milliseconds(pumps*25)','milliseconds((warming+pumps)*25)',1)
 text+=r'''
int RunOriginalWindowsGame(void* module,void* hwnd,std::uintptr_t api,std::uintptr_t raw,
 platform3g::Window& window,platform3g::Device& device){
 HostClean();MatrixClean();const auto cwd=std::filesystem::current_path();
 diagnostic::Journal journal(std::filesystem::path(output_root)/"journals","E591-native-Rangers-Run",hosted_map_source_sha);diagnostic::Binding journal_binding(journal);
 {
  RangersHostSession host;auto* config=GR_Main::MainDataConfig;auto* data=GR_Main::CacheDataRoot;
  NativeRangersServicesScope services(host.Table());output_paths::NativeScope outputs(output_root.c_str());
  auto defaults=SRobotsSettings::generate_default_settings();defaults.FDirect3D=reinterpret_cast<void*>(api);defaults.FD3DDevice=reinterpret_cast<void*>(raw);defaults.m_ResolutionX=800;defaults.m_ResolutionY=600;
  std::wstring copied_root=matrix_root;OriginalGameOptions options;options.module=module;options.window=hwnd;options.seed=37;
  options.window_provider=&window;options.device_provider=&device;options.resource_root=copied_root.c_str();options.settings=&defaults;
  Robot::TRobotRun detached;
  {
   NativeRangersRunBinding binding(options,RangersLegacyReferenceTag{});const auto run=binding.Entry();detached=run;
   copied_root=L"NOT_A_RESOURCE_ROOT";defaults.m_ResolutionX=1;defaults.FDirect3D=nullptr;
   bool duplicate=false;try{NativeRangersRunBinding other(options,RangersLegacyReferenceTag{});}catch(const std::logic_error&){duplicate=true;}assert(duplicate);
   aGalaxyStruct::TPlanetBattleStatistics output{};output.SignedTimeMs=11;output.RobotsBuilt=22;output.RobotsDestroyed=33;output.TurretsBuilt=44;output.TurretsDestroyed=55;output.BuildingsDestroyed=66;
   bool rejected=false;try{run(0,0,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);MatrixClean();
   char16_t absent[]=u"\u043d\u0435\u0442_\U0001f680_e591.cmap";
   rejected=false;try{run(0,0,absent,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&output);}catch(...){rejected=true;}assert(rejected);MatrixClean();
   assert(output.SignedTimeMs==11 && output.RobotsBuilt==22 && output.RobotsDestroyed==33 && output.TurretsBuilt==44 && output.TurretsDestroyed==55 && output.BuildingsDestroyed==66);
   std::puts("RANGERS_RUN null_output_and_Init_failure=REJECTED output_unchanged=PASS cleanup_retry_ready=PASS copied_context=PASS");
   Driver backend;native_loop::Scope loop(backend);
   // A null settings request uses the owned original defaults, which were
   // deliberately changed by the caller after constructing the binding.
   const int code=run(0xffffffffu,0xdeadbeefu,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&output);
   MatrixClean();assert(code==2 && backend.natural_outcome==2 && backend.warming>0);
   assert(output.SignedTimeMs==backend.expected.m_Time && output.RobotsBuilt==backend.expected.m_BuildRobot && output.RobotsDestroyed==backend.expected.m_KillRobot && output.TurretsBuilt==backend.expected.m_BuildTurret && output.TurretsDestroyed==backend.expected.m_KillTurret && output.BuildingsDestroyed==backend.expected.m_KillBuilding);
   assert(EngineCallbacks::Bound() && GR_Main::MainDataConfig==config && GR_Main::CacheDataRoot==data && g_RangersInterface);
   std::printf("RANGERS_RUN_RESULT code=%d signed_time=%d robots_destroyed=%d turrets_destroyed=%d warming=%u draws=%u ticks=%u typed_Run=PASS original_stats=PASS owner_cleanup_before_return=PASS host_alive=PASS legacy_tokens_not_pointer_casts=PASS\n",code,output.SignedTimeMs,output.RobotsDestroyed,output.TurretsDestroyed,backend.warming,backend.draws,backend.extra_ticks);
  }
  bool rejected=false;try{detached(0,0,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);}catch(const std::logic_error&){rejected=true;}assert(rejected);
  assert(std::filesystem::current_path()==cwd);std::puts("HOSTED_MAP full_Init_four_frames_Deinit=PASS real_Rangers_owner_survives=PASS cwd_unchanged=PASS");
 }
 HostClean();MatrixClean();assert(!output_paths::NativeBound());journal.RequireHealthy();std::puts("HOSTED_MAP final_host_cleanup=PASS");return 0;
}
'''
 probe=BASE/'rangers_run_reference.cpp'
 if probe.exists():assert probe.read_text(encoding='utf-8')==text
 else:probe.write_text(text,encoding='utf-8')
 result={'status':'INCOMPLETE','commands':[],'Switch':'NOT RUN','galaxy_transition':'NOT RUN'}
 def run(command,label,cwd=ROOT):
  command=list(map(str,command));p=subprocess.run(command,cwd=cwd,env=env,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=300);log=p.stdout+p.stderr;path=BASE/(label+'.log');index=1
  while path.exists():path=BASE/f'{label}-{index}.log';index+=1
  path.write_text(log,encoding='utf-8');result['commands'].append({'label':label,'command':command,'exit':p.returncode,'log':str(path.relative_to(ROOT))});print(label,p.returncode,log[-5000:] if p.returncode else '',flush=True);assert not p.returncode;return log
 try:
  (BASE/'hosted_map_build.hpp').write_text('inline constexpr char hosted_map_source_sha[]="'+sha(probe)+'";\n',encoding='utf-8')
  flags=[f.replace(str(fixed),str(BASE)) for f in prior['flags']]
  flags+=['/I'+str(ROOT/'port/matrixgame/native-engine'),'/fsanitize=address','/D_DISABLE_VECTOR_ANNOTATION','/D_DISABLE_STRING_ANNOTATION'];result['flags']=flags
  sources=[probe,ROOT/'port/matrixgame/native-engine/native_rangers_run.cpp'];objects=[]
  for source in sources:
   obj=BASE/(source.stem+'.obj');run([compiler(env),*flags,'/c',source,'/Fo'+str(obj)],'compile-'+source.stem);objects.append(obj)
  inputs=[Path(line.strip('"')) for line in (fixed/'link.rsp').read_text().splitlines()]
  inputs=[p for p in inputs if p.name!='completed_loop_legacy_audio.obj'];inputs+=objects
  response=BASE/'link.rsp';response.write_text('\n'.join('"'+str(p)+'"' for p in inputs),encoding='utf-8');binary=BASE/'rangers-run.exe'
  command=next(c['command'] for c in prior['commands'] if c['label']=='link');command=[('@'+str(response)) if s.startswith('@') else ('/Fe'+str(binary)) if s.startswith('/Fe') else s for s in command];command.insert(1,'/fsanitize=address');run(command,'link')
  # The genuine original SDK main still runs both full sessions and validates
  # window/gamma/device/timer restoration. Keep the local official SDK DLL path.
  env['PATH']=str(fixed)+';'+env['PATH'];runtime=BASE/'runtime';runtime.mkdir(exist_ok=True);output=runtime/'output';output.mkdir(exist_ok=True)
  result['stdout']=run([binary,output,ROOT/'windows/Space Rangers HD A War Apart',ROOT/'build/matrixgame/e2e578-hosted-windows-native/prefix/native-input'],'runtime',runtime)
  assert result['stdout'].count('typed_Run=PASS original_stats=PASS owner_cleanup_before_return=PASS host_alive=PASS legacy_tokens_not_pointer_casts=PASS')==2
  result.update(status='PASS; PARTIAL MSVC ASAN TYPED RANGERS RUN ENTRY EXECUTED TWO COMPLETE ORIGINAL MISSION LOSSES; NO GALAXY TRANSITION',sources_sha256={str(p.relative_to(ROOT)):sha(p) for p in [*sources,ROOT/'port/matrixgame/native-engine/native_rangers_run.hpp',Path(__file__).resolve()]},link_inputs_sha256={str(p):sha(p) for p in inputs},binary_sha256=sha(binary))
 finally:
  path=BASE/'result.json';index=1
  while path.exists():path=BASE/f'result-{index}.json';index+=1
  path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':main()
