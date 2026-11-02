"""Real complete Robot interface lifecycle and original Rangers Finalize caller."""
import subprocess,sys
from matrixgame_standard_nro_audit import ROOT
if __name__=='__main__':
 source=ROOT/'build/matrixgame/e2e591-native-rangers-run/settings-fixture/run.py';text=source.read_text(encoding='utf-8')
 replacements={
  "BASE=ROOT/'build/matrixgame/e2e591-native-rangers-run/windows-asan-settings'":"BASE=ROOT/'build/matrixgame/e2e592-native-rangers-interface/windows-asan-fixed'",
  '#include "native_rangers_run.hpp"':'#include "native_rangers_run.hpp"\\n#include "native_rangers_interface.hpp"\\n#include "native_rangers_support.hpp"\\n#include "rangers_host_callbacks.hpp"',
  '  RangersHostSession host;auto* config':'''  {RangersHostSession ordinary;assert(EngineCallbacks::Bound());const float before=ordinary.Table().m_MusicVolumeGet();ordinary.Table().m_MusicVolumeSet(0.25f);assert(ordinary.Table().m_MusicVolumeGet()==0.25f);ordinary.Table().m_MusicVolumeSet(before);}HostClean();
  std::puts("ROBOT_INTERFACE default_host_callbacks_regression=PASS");
  RangersHostSession host(RangersHostOptions{},RangersDeferredCallbacksTag{});assert(!EngineCallbacks::Bound());auto* config''',
  '  NativeRangersServicesScope services(host.Table());output_paths::NativeScope outputs':'  output_paths::NativeScope outputs',
  '   NativeRangersRunBinding binding(options,RangersLegacyReferenceTag{});const auto run=binding.Entry();detached=run;':'''   NativeRangersInterface binding(options,RangersLegacyReferenceTag{});
   const auto facts=QueryRangersSupport(device);assert(ClassifyRangersSupport(facts)==0 && binding.Table().Support()==0);
   std::printf("ROBOT_INTERFACE real_support_query=PASS textures=%u width=%u streams=%u\\n",facts.simultaneous_textures,facts.max_texture_width,facts.streams);
   assert(!EngineCallbacks::Bound() && !g_RangersInterface && !Robot::RobotInterface && !Robot::RobotModule);
   bool early=false;try{binding.Table().Run(0,0,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);}catch(const std::logic_error&){early=true;}assert(early);
   auto callbacks=MakeRangersHostCallbacks();auto bad=callbacks;bad.FreeText=Robot::TRobotFreeText{};
   bool rejected_init=false;try{binding.Install(bad);}catch(const std::invalid_argument&){rejected_init=true;}assert(rejected_init && !EngineCallbacks::Bound() && !g_RangersInterface && !Robot::RobotInterface);
   const auto before_callbacks=callbacks;binding.Install(callbacks);assert(std::memcmp(&before_callbacks,&callbacks,sizeof(callbacks))==0);
   auto* installed=Robot::RobotInterface;assert(installed && pas::assigned(installed->Initialize) && pas::assigned(installed->Finalize) && pas::assigned(installed->Support) && pas::assigned(installed->Run));
   assert(EngineCallbacks::Bound() && g_RangersInterface && installed->Support()==0);
   const auto run=installed->Run;detached=run;
   std::puts("ROBOT_INTERFACE all_four_real_slots=PASS invalid_Initialize_rollback=PASS callbacks_readonly=PASS global_install=PASS");''',
  'NativeRangersRunBinding other(options,RangersLegacyReferenceTag{})':'NativeRangersInterface other(options,RangersLegacyReferenceTag{})',
  '  }\n  bool rejected=false;try{detached':'''   Robot::FinalizeRobotRuntime();assert(!Robot::RobotInterface && !Robot::RobotModule && !EngineCallbacks::Bound() && !g_RangersInterface);MatrixClean();
   bool after_finalize=false;try{run(0,0,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);}catch(const std::logic_error&){after_finalize=true;}assert(after_finalize);
   binding.Install(callbacks);assert(Robot::RobotInterface && Robot::RobotInterface->Support()==0);
   bool duplicate_init=false;try{Robot::RobotInterface->Initialize(&callbacks);}catch(const std::logic_error&){duplicate_init=true;}assert(duplicate_init && EngineCallbacks::Bound());
   Robot::FinalizeRobotRuntime();assert(!Robot::RobotInterface && !Robot::RobotModule && !EngineCallbacks::Bound() && !g_RangersInterface);
   std::puts("ROBOT_INTERFACE original_FinalizeRobotRuntime=PASS Run_after_Finalize_rejected=PASS reinitialize=PASS duplicate_Initialize_rejected=PASS");
  }
  bool rejected=false;try{detached''',
  "sources=[probe,ROOT/'port/matrixgame/native-engine/native_rangers_run.cpp',main_source]":"sources=[probe,ROOT/'port/matrixgame/native-engine/native_rangers_run.cpp',main_source,*[ROOT/'port/matrixgame/native-engine'/name for name in ('native_rangers_interface.cpp','native_rangers_support.cpp')],ROOT/'port/matrixgame/native/rangers_host_session.cpp']",
  "('completed_loop_legacy_audio.obj','hosted_main.obj')":"('completed_loop_legacy_audio.obj','hosted_main.obj','rangers_host_session.obj')",
  'ONE COMPLETE ORIGINAL MISSION LOSS WITH NONNULL CALLER SETTINGS':'COMPLETE REAL ROBOT INTERFACE LIFECYCLE AND ONE ORIGINAL MISSION LOSS',
 }
 for before,after in replacements.items():assert text.count(before)==1,before;text=text.replace(before,after,1)
 out=ROOT/'build/matrixgame/e2e592-native-rangers-interface/windows-fixture';out.mkdir(parents=True,exist_ok=True);script=out/'run-assigned-fixed.py'
 if script.exists():assert script.read_text(encoding='utf-8')==text
 else:script.write_text(text,encoding='utf-8')
 subprocess.run([sys.executable,script],check=True)
