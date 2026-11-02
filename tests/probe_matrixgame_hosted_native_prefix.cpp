// SPDX-License-Identifier: GPL-2.0-or-later
// Actual original Init rollback with real host callbacks; no success/device emulation.
#include "init_runtime.hpp"
#include "MatrixGame.h"
#include "MatrixLoadProgress.hpp"
#include "rangers_host_session.hpp"
#include "engine_callbacks.hpp"
#include "runtime_support.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_Sound.hpp"
#include "units/EC_HsFile.hpp"
#include "ec_file_adapter.hpp"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <codecvt>
#include <locale>
using namespace srhd_awa::matrixgame;namespace p=srhd_awa::matrixgame::platform3g;
struct Window final:p::Window {
 unsigned attachments{};
 void* Identity()const noexcept override{return const_cast<Window*>(this);}
 p::WindowRect ReadClientRect()override{return {0,0,1280,720};}
 struct Lease final:p::WindowAttachment {Window& owner;explicit Lease(Window& w):owner(w){++owner.attachments;}void Restore()noexcept override{}~Lease(){--owner.attachments;}};
 std::unique_ptr<p::WindowAttachment> AttachNative()override{return std::make_unique<Lease>(*this);}
 std::unique_ptr<p::WindowAttachment> Attach(p::MessageHandler)override{throw p::Unsupported("no legacy window");}
 void Show(p::Visibility)override{throw p::Unsupported("no host window display");}
 void ReleaseCursor()override{throw p::Unsupported("no host cursor");}
 std::intptr_t DefaultMessage(std::uint32_t,std::uintptr_t,std::intptr_t)override{throw p::Unsupported("no Win32 messages");}
};
struct Device final:p::Device {
 const SMGDRangersInterface* caller{};unsigned caps{},raster_calls{};std::uint32_t sound{};
 p::DeviceCaps ReadCaps()override{
  ++caps;assert(g_MatrixHeap && g_MatrixData && GR_Main::MainDataConfig && GR_Main::SoundManager);
  assert(g_RangersInterface && g_RangersInterface!=caller && g_RangersInterface->m_RangersText==caller->m_RangersText);
  CLoadProgress progress;progress.SetCurLP(0);progress.InitCurLP(100);progress.SetCurLPPos(50);
  wchar_t text[]=L"Original Init host",font[]=L"Font.2Normal";SMGDRangersInterfaceText image{};
  g_RangersInterface->m_RangersText(text,font,0xffffffff,200,64,0,0,0,0,0,nullptr,&image);assert(image.m_Buf);g_RangersInterface->m_RangersTextClear(&image);++raster_calls;
  wchar_t path[]=L"Sound.FormShipClose";sound=g_RangersInterface->m_SoundCreate(path,0,1);assert(sound);
  g_RangersInterface->m_SoundVolume(sound,.4f);assert(g_RangersInterface->m_SoundIsPlay(sound));
  throw p::Unsupported("HOSTED_NATIVE_CAPS_BOUNDARY");
 }
 p::GammaRamp ReadGamma(unsigned)override{throw p::Unsupported("no host gamma");}
 void RestoreGamma(unsigned,const p::GammaRamp&)noexcept override{std::terminate();}
 std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("no host GPU memory");}
};
void NativeClean(){assert(!g_RangersInterface && !g_MatrixHeap && !g_MatrixData && !g_Cache && !g_CacheHeap && !g_MatrixMap && !output_paths::NativeBound());}
int main(int argc,char** argv){
 assert(argc==4);std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 const auto root=std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(argv[3]);
 const auto output=std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(std::filesystem::absolute("native-output").string());
 NativeClean();Window window;Device device;
 for(int cycle=0;cycle<2;++cycle){
  {
   RangersHostSession host;device.caller=&host.Table();
   auto* config=GR_Main::MainDataConfig;auto* cache=GR_Main::CacheDataRoot;
   NativeInitRuntime runtime;NativeGameOptions options{window,device};options.seed=37;options.resource_root=root.c_str();options.output_root=output.c_str();options.services=&host.Table();
   bool stopped=false;try{runtime.InitializeNativeGame(options);}catch(const p::Unsupported& error){stopped=std::string(error.what())=="HOSTED_NATIVE_CAPS_BOUNDARY";}
   catch(const Base::CException& error){const auto text=std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(error.Info());std::fprintf(stderr,"HOSTED_NATIVE_BASE_ERROR %s\n",text.c_str());throw;}
   assert(stopped && !runtime.CpuReady() && !runtime.OriginalGameReady() && !window.attachments);NativeClean();
   assert(EngineCallbacks::Bound() && GR_Main::MainDataConfig==config && GR_Main::CacheDataRoot==cache && GR_Main::SoundManager);
   assert(host.Table().m_SoundIsPlay(device.sound));host.Table().m_SoundDestroy(device.sound);device.sound=0;
   runtime.Deinit();NativeClean();assert(EngineCallbacks::Bound() && GR_Main::MainDataConfig==config);
  }
  assert(!EngineCallbacks::Bound() && !GR_Main::MainDataConfig && !GR_Main::CacheDataRoot && !GR_Main::SoundManager && !GR_Main::MusicManager && !EC_HsFile::PackageCollection);NativeClean();
 }
 assert(device.caps==2 && device.raster_calls==2);
 std::puts("HOSTED_NATIVE original_Init_caps_cycles=2 real_text_callbacks=2 real_sound_callbacks=PASS copied_table=PASS host_survives_rollback=PASS host_cleanup=PASS NO_GPU NO_FULL_INIT NO_SWITCH");return 0;
}
