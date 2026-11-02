// Real PC HAL Init/map/frames with the actual owning Rangers callback host.
// Windows SDK owner/readback is kept in its existing separate translation unit.
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "MatrixConfig.hpp"
#include "MatrixMap.hpp"
#include "skin_callbacks.hpp"
#include "CException.hpp"
#include "platform_win32.hpp"
#include "offscreen_backend.hpp"
#include "resource_paths.hpp"
#include "native_rangers_services.hpp"
#include "native_diagnostic_journal.hpp"
#include "rangers_host_session.hpp"
#include "engine_callbacks.hpp"
#include "runtime_support.hpp"
#include "units/GR_Main.hpp"
#include "units/EC_HsFile.hpp"
#include "ec_file_adapter.hpp"
#include "hosted_map_build.hpp"
#include <cassert>
#include <cstdio>
#include <codecvt>
#include <filesystem>
#include <locale>
using namespace srhd_awa::matrixgame;
namespace {
std::wstring matrix_root,output_root;
void HostClean(){assert(!EngineCallbacks::Bound() && !GR_Main::MainDataConfig && !GR_Main::CacheDataRoot && !GR_Main::SoundManager && !GR_Main::MusicManager && !GR_Main::GlobalCache && !EC_HsFile::PackageCollection && !g_RangersInterface);}
void MatrixClean(){assert(!g_MatrixMap && !g_MatrixHeap && !g_MatrixData && !g_Cache && !g_CacheHeap && !g_Render && !g_LoadProgress && !BorrowedSkinLeaseCount() && !InspectNativeDrawSink() && !resource_paths::Bound());}
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
 diagnostic::Journal journal(std::filesystem::path(output_root)/"journals","E579-Windows-HAL-hosted",hosted_map_source_sha);
 diagnostic::Binding journal_binding(journal);
 {
  RangersHostSession host;auto* config=GR_Main::MainDataConfig;auto* data=GR_Main::CacheDataRoot;
  NativeRangersServicesScope services(host.Table());const auto* copied=g_RangersInterface;
  assert(copied && copied!=&host.Table() && copied->m_RangersText==host.Table().m_RangersText);
  output_paths::NativeScope private_outputs(output_root.c_str());
  NativeInitRuntime runtime;
  auto settings=SRobotsSettings::generate_default_settings();settings.FDirect3D=reinterpret_cast<void*>(api);settings.FD3DDevice=reinterpret_cast<void*>(raw);
  settings.m_ResolutionX=800;settings.m_ResolutionY=600;
  OriginalGameOptions options;options.module=module;options.window=hwnd;options.seed=37;
  options.window_provider=&window;options.device_provider=&device;options.settings=&settings;options.resource_root=matrix_root.c_str();
  try{
   runtime.InitializeOriginalGame(options);assert(runtime.OriginalGameReady() && g_MatrixMap);
   assert(g_MatrixMap->m_Size.x>0 && g_MatrixMap->m_Size.y>0 && g_MatrixMap->m_GroupSize.x>0 && g_MatrixMap->m_GroupSize.y>0);
   assert(EngineCallbacks::Bound() && GR_Main::MainDataConfig==config && GR_Main::CacheDataRoot==data && g_RangersInterface==copied);
   const auto rect=window.ReadClientRect();assert(rect.right-rect.left==800 && rect.bottom-rect.top==600);
   std::printf("HOSTED_MAP original_Init=PASS map=%dx%d groups=%dx%d copied_table=PASS\n",g_MatrixMap->m_Size.x,g_MatrixMap->m_Size.y,g_MatrixMap->m_GroupSize.x,g_MatrixMap->m_GroupSize.y);
   auto& sdk=dynamic_cast<platform3g::D3D9Device&>(device);const auto before=sdk.InspectDrawStats();
   {
    unsigned transition_frames{};
    // Real hosted Init captures its loading window and starts the original
    // moving-tile transition. Advance the actual Takt/Draw chain until it ends;
    // keep the nonuniform map readback assertion unchanged after that point.
    while((g_MatrixMap->m_Flags&MMFLAG_TRANSITION) && transition_frames<200){
     g_MatrixMap->Takt(25);DrawOriginalWindowsFrame(nullptr);++transition_frames;
    }
    assert(!(g_MatrixMap->m_Flags&MMFLAG_TRANSITION));
    std::printf("HOSTED_MAP original_transition_takt_draw_frames=%u completed=PASS\n",transition_frames);
    for(unsigned frame=0;frame<4;++frame){g_MatrixMap->Takt(25);DrawOriginalWindowsFrame(CaptureOriginalMapFrame);std::printf("HOSTED_MAP frame=%u readback_present=PASS\n",frame);}
   }
   const auto after=sdk.InspectDrawStats();assert(after.indexed>before.indexed && after.calls>before.calls);
   runtime.Deinit();assert(runtime.LastStateRestoreResult()==platform3g::DeviceSuccess);MatrixClean();
   assert(EngineCallbacks::Bound() && GR_Main::MainDataConfig==config && GR_Main::CacheDataRoot==data && g_RangersInterface==copied);
   assert(std::filesystem::current_path()==cwd);runtime.Deinit();MatrixClean();
   std::puts("HOSTED_MAP full_Init_four_frames_Deinit=PASS real_Rangers_owner_survives=PASS cwd_unchanged=PASS");result=0;
  }catch(const Base::CException& error){std::printf("HOSTED_MAP CException %s\n",std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(error.Info()).c_str());}
  catch(const std::exception& error){std::printf("HOSTED_MAP boundary %s\n",error.what());}
  if(result && platform3g::Current().ready)offscreen::EndScene();
  runtime.Deinit();MatrixClean();
 }
 HostClean();assert(!output_paths::NativeBound());journal.RequireHealthy();
 std::puts("HOSTED_MAP final_host_cleanup=PASS");return result;
}
