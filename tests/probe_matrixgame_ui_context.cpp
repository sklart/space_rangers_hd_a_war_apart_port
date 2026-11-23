#include "rangers_ui_context.hpp"
#include "rangers_host_session.hpp"
#include "rangers_script_session.hpp"
#include "rangers_global_ui_session.hpp"
#include "rangers_world_shutdown.hpp"
#include "ec_file_adapter.hpp"
#include "runtime_support.hpp"
#include "renderer_platform.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Buf.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_DX.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/aMyFunction.hpp"
#include "units/aGalaxy.hpp"
#include "units/aScript.hpp"
#include "units/aSaveLoad.hpp"
#include <cassert>
#include <cstdio>
#include <cerrno>
#include <type_traits>
#include <filesystem>
#include <fstream>
namespace m=srhd_awa::matrixgame;
namespace renderer=srhd_awa::platform::renderer_platform;
#ifdef MATRIXGAME_UI_CONTEXT_REALLOC_WRAP
bool fail_late{},late_failure_seen{};
extern "C" void* __real_realloc(void*,std::size_t);
extern "C" void* __real_malloc(std::size_t);
bool FailLate(std::size_t size){
 const auto pending_bytes=2*sizeof(pas::Integer)+1024*sizeof(std::remove_reference_t<decltype(GR_DX::PendingPoints[0])>);
 if(fail_late && size==pending_bytes && GR_Main::CurrentPixelFormat && GR_Main::ScreenRenderBuffer && GR_Main::RenderScratchBuffer && GR_Main::AuxRenderBuffer){fail_late=false;late_failure_seen=true;errno=ENOMEM;return true;}
 return false;
}
extern "C" void* __wrap_malloc(std::size_t size){if(FailLate(size))return nullptr;return __real_malloc(size);}
extern "C" void* __wrap_realloc(void* p,std::size_t size){
 if(!p && FailLate(size))return nullptr;
 return __real_realloc(p,size);
}
#endif
template<class F>void Reject(F&& f){bool rejected=false;try{f();}catch(...){rejected=true;}assert(rejected);}
void Empty(){
 assert(!renderer::IsInitialized() && !GR_Main::CurrentPixelFormat && !GR_Main::ScreenRenderBuffer && !GR_Main::RenderScratchBuffer && !GR_Main::AuxRenderBuffer && !GR_Main::UserSettingsConfig && !GR_Main::NewGameSettingsConfig && !GR_Main::EditableSaveBlock && !GR_Main::CCInterface);
 assert(!GR_Main::UiStyleConfig && !GR_Main::GameDataConfig && !GR_Main::UiDepthConfig && !GR_Main::InterfaceBlendPalette && !GR_Main::FirstRegisteredCursor && !GR_Main::LastRegisteredCursor && !aGalaxy::Galaxy);
 assert(!Globals::TurnCalculationThread && !Globals::GovernmentScreen && !GR_DX::PendingPoints.length());
}
int main(int argc,char** argv){
 assert(argc==3);std::setvbuf(stdout,nullptr,_IONBF,0);srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 Empty();Reject([]{m::RangersUiContext no_host;});Empty();
 {
  m::RangersHostOptions options;options.sound=false;options.music=false;m::RangersHostSession host(options);
  const auto width=GR_Main::GameScreenWidth,height=GR_Main::GameScreenHeight;
  const auto presentation=GR_Main::PresentationWidth;const auto planet_depth=GlobalsV::PlanetDepth;
  const auto tags=aMyFunction::InfoNameColorTag;const auto blend=GR_Main::BlendPixel16;
  m::RangersUiContextOptions missing;missing.settings_path=u"CFG/matrixgame-ui-context-absent-input.txt";
  Reject([&]{m::RangersUiContext context(missing);});Empty();
  auto* foreign=pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  GR_Main::EditableSaveBlock=foreign;Reject([]{m::RangersUiContext context;});
  assert(GR_Main::EditableSaveBlock==foreign);GR_Main::EditableSaveBlock=nullptr;pas::free(foreign);Empty();
#ifdef MATRIXGAME_UI_CONTEXT_REALLOC_WRAP
  fail_late=true;Reject([]{m::RangersUiContext context;});assert(late_failure_seen && !fail_late);Empty();
  assert(GR_Main::GameScreenWidth==width && GR_Main::PresentationWidth==presentation && GlobalsV::PlanetDepth==planet_depth && aMyFunction::InfoNameColorTag==tags && GR_Main::BlendPixel16==blend);
  std::puts("UI_CONTEXT actual_late_realloc_failure=PASS three_real_buffers_released=PASS globals_restored=PASS");
#endif
  for(int cycle=0;cycle<2;++cycle){
   {
    m::RangersUiContext context;assert(context.Settings().GetParamCount()>0 && GR_Main::UserSettingsConfig==&context.Settings());
    assert(GR_Main::EditableSaveBlock);GR_Main::EditableSaveBlock->AddParam(u"probe"_wref.get(),u"value"_wref.get());
    assert(GR_Main::EditableSaveBlock->CountParams(u"probe"_wref.get())==1);GR_Main::EditableSaveBlock->Clear();
    assert(GR_Main::CCInterface && GR_Main::ScreenRenderBuffer->PitchBytes==2048 && GR_Main::ScreenRenderBuffer->GetPixels() && GR_Main::GameScreenWidth==1024 && GR_Main::GameScreenHeight==768);
    assert(GR_Main::InterfaceBlendPalette && GR_Main::FirstRegisteredCursor && GR_Main::LastRegisteredCursor && GR_Main::UiDepthConfig && aMyFunction::InfoNameColorTag.length()>0);
    auto* cursor=GR_Main::FirstRegisteredCursor;assert(GR_Main::FindCursorByName(pas::view(cursor->Name))==cursor && cursor->ImagePath.length()>0);
    Reject([]{m::RangersUiContext duplicate;});assert(GR_Main::UserSettingsConfig==&context.Settings());
    {m::RangersScriptSession scripts;m::RangersGlobalUiSession ui;assert(Globals::GovernmentScreen && Globals::MainMenuScreen && Globals::TurnCalculationThread);
     GlobalsV::MemorySnapshotActive=true;Reject([&]{m::ShutdownRangersWorld(scripts);});
     assert(GlobalsV::MemorySnapshotActive && aScript::ScriptRequestThread);GlobalsV::MemorySnapshotActive=false;
     auto* snapshot=pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);aSaveLoad::MemorySnapshotBuffer=snapshot;
     Reject([&]{m::ShutdownRangersWorld(scripts);});assert(aSaveLoad::MemorySnapshotBuffer==snapshot && aScript::ScriptRequestThread);
     aSaveLoad::MemorySnapshotBuffer=nullptr;pas::free(snapshot);
     auto* worker=aScript::ScriptRequestThread;aScript::ScriptRequestThread=nullptr;
     Reject([&]{scripts.StopRequestWorker();});assert(!aScript::ScriptRequestThread);aScript::ScriptRequestThread=worker;
     aScript::StartScriptRequestThread();scripts.StopRequestWorker();assert(!aScript::ScriptRequestThread);
     scripts.StopRequestWorker();assert(Globals::GovernmentScreen && Globals::MainMenuScreen);}
    std::printf("UI_CONTEXT cycle=%d original_CFG_TCC_canvas_palette_cursors=PASS original_script_global_UI=PASS duplicate_rejected=PASS\n",cycle);
   }
   Empty();assert(GR_Main::GameScreenWidth==width && GR_Main::GameScreenHeight==height && GR_Main::PresentationWidth==presentation && GlobalsV::PlanetDepth==planet_depth && aMyFunction::InfoNameColorTag==tags && GR_Main::BlendPixel16==blend);
  }
  // These files exist only in the isolated test root supplied by the runner.
  const auto config=std::filesystem::path(argv[2])/"config";
  const auto cfg=config/"cfg.txt", newgame=config/"NEWGAME.TXT";
  assert(!std::filesystem::exists(cfg) && !std::filesystem::exists(newgame));
  {std::ofstream f(cfg);f<<"UiContextUserProbe=Present\nSound=True\nMusic=True\nSoundVolume=37\nRobotSoundVolume=-8\nMusicVolume=170\nRobotMusicVolume=24\nPathGrow=False\nMultiThread=True\n";assert(f.good());}
  {std::ofstream f(newgame);f<<"UiContextNewGameProbe=Present\n";assert(f.good());}
  const auto cfg_size=std::filesystem::file_size(cfg),newgame_size=std::filesystem::file_size(newgame);
  const auto old_sound=GlobalsV::SoundEnabled,old_music=GlobalsV::MusicEnabled,old_multi=GlobalsV::MultiThreadEnabled;
  const auto old_volume=GlobalsV::SoundVolume,old_robot_volume=GlobalsV::RobotSoundVolume,old_music_volume=GlobalsV::MusicVolume,old_robot_music_volume=GlobalsV::RobotMusicVolume;
  const auto old_path=GR_Main::PathGrowEnabled;
  {
   m::RangersUiContext context;
   assert(context.Settings().CountParams(u"UiContextUserProbe"_wref.get())==1);
   assert(GR_Main::NewGameSettingsConfig->CountParams(u"UiContextNewGameProbe"_wref.get())==1);
   assert(!GlobalsV::SoundEnabled && !GlobalsV::MusicEnabled); // Host intentionally has no audio workers.
   assert(GlobalsV::SoundVolume==0.37f && GlobalsV::RobotSoundVolume==0.0f && GlobalsV::MusicVolume==1.0f && GlobalsV::RobotMusicVolume==0.24f);
   assert(!GR_Main::PathGrowEnabled && GlobalsV::MultiThreadEnabled);
   std::puts("UI_CONTEXT user_cfg_and_newgame_case_insensitive=PASS");
  }
  assert(GlobalsV::SoundEnabled==old_sound && GlobalsV::MusicEnabled==old_music && GlobalsV::MultiThreadEnabled==old_multi && GR_Main::PathGrowEnabled==old_path);
  assert(GlobalsV::SoundVolume==old_volume && GlobalsV::RobotSoundVolume==old_robot_volume && GlobalsV::MusicVolume==old_music_volume && GlobalsV::RobotMusicVolume==old_robot_music_volume);
  std::puts("UI_CONTEXT settings_applied_and_restored=PASS");
  Empty();assert(std::filesystem::file_size(cfg)==cfg_size && std::filesystem::file_size(newgame)==newgame_size);
 }
 Empty();assert(!GR_Main::MainDataConfig && !GR_Main::GlobalCache);std::puts("UI_CONTEXT cleanup=PASS no_config_or_save_write_API=PASS");
}
