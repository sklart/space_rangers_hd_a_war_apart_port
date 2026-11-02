// Real owning Rangers services and actual PC HAL; deterministic input/clock only.
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "MatrixConfig.hpp"
#include "MatrixMap.hpp"
#include "MatrixFormGame.hpp"
#include "skin_callbacks.hpp"
#include "platform_win32.hpp"
#include "offscreen_backend.hpp"
#include "resource_paths.hpp"
#include "native_rangers_services.hpp"
#include "native_diagnostic_journal.hpp"
#include "native_form_backend.hpp"
#include "native_loop.hpp"
#include "native_input_engine.hpp"
#include "applet_engine_events.hpp"
#include "rangers_host_session.hpp"
#include "engine_callbacks.hpp"
#include "runtime_support.hpp"
#include "units/GR_Main.hpp"
#include "units/EC_HsFile.hpp"
#include "ec_file_adapter.hpp"
#include "hosted_map_build.hpp"
#include "CException.hpp"
#include "input.hpp"
#include <cassert>
#include <cstdio>
#include <codecvt>
#include <filesystem>
#include <locale>
using namespace srhd_awa::matrixgame;
namespace {
std::wstring matrix_root,output_root;
void HostClean(){assert(!EngineCallbacks::Bound() && !GR_Main::MainDataConfig && !GR_Main::CacheDataRoot && !GR_Main::SoundManager && !GR_Main::MusicManager && !GR_Main::GlobalCache && !EC_HsFile::PackageCollection && !g_RangersInterface);}
void MatrixClean(){assert(!g_MatrixMap && !g_MatrixHeap && !g_MatrixData && !g_Cache && !g_CacheHeap && !g_Render && !g_LoadProgress && !BorrowedSkinLeaseCount() && !InspectNativeDrawSink() && !resource_paths::Bound() && !g_FormCur && !g_FormFirst && !g_FormLast);}
struct Driver final:native_loop::Backend {
 native_input::EngineSink input;platform3g::AppletEngineEvents events;
 unsigned pumps{},draws{};float camera_x{},camera_y{};int start_time{};
 void Key(std::uint8_t key){input.Key(true,key);input.Key(false,key);}
 void Pump()override{
  ++pumps;
  switch(pumps){
  case 1:events.FocusChanged(true);input.Move(400,300);assert(g_MatrixMap->IsPaused());start_time=g_MatrixMap->GetTime();break;
  case 2:Key(13);assert(!g_MatrixMap->IsPaused());break;
  case 3:Key(19);assert(g_MatrixMap->IsPaused());break;
  case 4:Key(19);assert(!g_MatrixMap->IsPaused());break;
  case 5:Key(27);assert(g_MatrixMap->IsPaused());break;
  case 6:Key(27);assert(!g_MatrixMap->IsPaused());break;
  case 7:{const auto xy=g_MatrixMap->m_Camera.GetXYStrategy();camera_x=xy.x;camera_y=xy.y;const auto key=g_Config.m_KeyActions[KA_SCROLL_RIGHT];assert(key);input.Key(true,key);assert(Input::isKeyPressed(KA_SCROLL_RIGHT));break;}
  case 8:input.Key(false,g_Config.m_KeyActions[KA_SCROLL_RIGHT]);assert(!Input::isKeyPressed(KA_SCROLL_RIGHT));break;
  case 209:g_Flags|=GFLAG_EXITLOOP;break;
  default:assert(pumps<209);assert(!g_MatrixMap->IsPaused());break;
  }
 }
 void Idle()override{throw std::runtime_error("hosted original form unexpectedly inactive");}
 void Draw(CForm& form)override{form.Draw();++draws;}
 native_loop::Clock::time_point Now()override{return native_loop::Clock::time_point{}+std::chrono::milliseconds(pumps*25);}
};
}
void ConfigureHostedRoots(const char* rangers,const char* matrix,const char* output){
 matrix_root=std::filesystem::path(matrix).wstring();output_root=std::filesystem::path(output).wstring();
 const auto user=std::filesystem::path(output)/"rangers-user";std::filesystem::create_directories(user);
 srhd_awa::platform::ec_file::SetGameRoot(rangers);srhd_awa::platform::ec_file::SetUserRoot(user.string());
}
void DrawOriginalWindowsFrame(void (*)());
void CaptureOriginalMapFrame();
int RunOriginalWindowsGame(void* module,void* hwnd,std::uintptr_t api,std::uintptr_t raw,
 platform3g::Window& window,platform3g::Device& device){
 HostClean();MatrixClean();const auto cwd=std::filesystem::current_path();int result=10;
 diagnostic::Journal journal(std::filesystem::path(output_root)/"journals","E581-Windows-HAL-hosted-loop",hosted_map_source_sha);
 diagnostic::Binding journal_binding(journal);
 {
  RangersHostSession host;auto* config=GR_Main::MainDataConfig;auto* data=GR_Main::CacheDataRoot;
  NativeRangersServicesScope services(host.Table());const auto* copied=g_RangersInterface;
  assert(copied && copied!=&host.Table() && copied->m_RangersText==host.Table().m_RangersText);
  output_paths::NativeScope private_outputs(output_root.c_str());NativeInitRuntime runtime;
  auto settings=SRobotsSettings::generate_default_settings();settings.FDirect3D=reinterpret_cast<void*>(api);settings.FD3DDevice=reinterpret_cast<void*>(raw);settings.m_ResolutionX=800;settings.m_ResolutionY=600;
  OriginalGameOptions options;options.module=module;options.window=hwnd;options.seed=37;
  options.window_provider=&window;options.device_provider=&device;options.settings=&settings;options.resource_root=matrix_root.c_str();
  try{
   runtime.InitializeOriginalGame(options);assert(runtime.OriginalGameReady() && g_MatrixMap);
   std::printf("HOSTED_MAP original_Init=PASS map=%dx%d groups=%dx%d copied_table=PASS\n",g_MatrixMap->m_Size.x,g_MatrixMap->m_Size.y,g_MatrixMap->m_GroupSize.x,g_MatrixMap->m_GroupSize.y);
   auto& sdk=dynamic_cast<platform3g::D3D9Device&>(device);const auto before=sdk.InspectDrawStats();
   unsigned transition_frames{};
   while((g_MatrixMap->m_Flags&MMFLAG_TRANSITION) && transition_frames<200){g_MatrixMap->Takt(25);DrawOriginalWindowsFrame(nullptr);++transition_frames;}
   assert(!(g_MatrixMap->m_Flags&MMFLAG_TRANSITION));
   std::printf("HOSTED_MAP original_transition_takt_draw_frames=%u completed=PASS\n",transition_frames);
   {
    native_form::Session form;assert(g_FormCur==&form.Form());Driver backend;native_loop::Scope scope(backend);g_Flags&=~GFLAG_EXITLOOP;
    assert(RunOriginalNativeLoop()==1);assert(backend.draws==208 && g_MatrixMap->GetTime()-backend.start_time==5125);
    const auto xy=g_MatrixMap->m_Camera.GetXYStrategy();assert(xy.x!=backend.camera_x || xy.y!=backend.camera_y);
    assert(g_MatrixMap->m_Cursor.GetPosX()==400 && g_MatrixMap->m_Cursor.GetPosY()==300);
    backend.events.FocusChanged(false);assert(!(g_Flags&GFLAG_APPACTIVE) && !(g_MatrixMap->m_Flags&MMFLAG_VIDEO_RESOURCES_READY));
    backend.events.FocusChanged(true);form.Form().Takt(25);assert(g_MatrixMap->m_Flags&MMFLAG_VIDEO_RESOURCES_READY);form.Form().Draw();
    assert(EngineCallbacks::Bound() && GR_Main::MainDataConfig==config && GR_Main::CacheDataRoot==data && g_RangersInterface==copied);
    std::puts("HOSTED_ORIGINAL_LOOP draws=208 elapsed=5125 input_pause_menu_camera=PASS focus_release_recreate=PASS host_callbacks_live=PASS");
    for(unsigned frame=0;frame<4;++frame){form.Form().Takt(25);DrawOriginalWindowsFrame(CaptureOriginalMapFrame);std::printf("HOSTED_MAP frame=%u readback_present=PASS\n",frame);}
   }
   assert(!g_FormCur && !g_FormFirst && !g_FormLast);
   const auto after=sdk.InspectDrawStats();assert(after.indexed>before.indexed && after.calls>before.calls);
   runtime.Deinit();assert(runtime.LastStateRestoreResult()==platform3g::DeviceSuccess);MatrixClean();
   assert(EngineCallbacks::Bound() && GR_Main::MainDataConfig==config && GR_Main::CacheDataRoot==data && g_RangersInterface==copied);
   assert(std::filesystem::current_path()==cwd);runtime.Deinit();MatrixClean();
   std::puts("HOSTED_MAP full_Init_four_frames_Deinit=PASS real_Rangers_owner_survives=PASS cwd_unchanged=PASS");result=0;
  }catch(const Base::CException& error){std::printf("HOSTED_LOOP CException %s\n",std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(error.Info()).c_str());}
  catch(const std::exception& error){std::printf("HOSTED_LOOP boundary %s\n",error.what());}
  if(result && platform3g::Current().ready)offscreen::EndScene();runtime.Deinit();MatrixClean();
 }
 HostClean();assert(!output_paths::NativeBound());journal.RequireHealthy();std::puts("HOSTED_MAP final_host_cleanup=PASS");return result;
}
