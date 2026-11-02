// SPDX-License-Identifier: GPL-2.0-or-later
// Full callback presence fixture. Only the original progress consumer executes;
// untested sound/text/music calls fail explicitly, never report fake success.
#include "init_runtime.hpp"
#include "native_rangers_services.hpp"
#include "MatrixLoadProgress.hpp"
#include "MatrixGame.h"
#include <cassert>
#include <cstdio>
#include <tuple>
#include <vector>
using namespace srhd_awa::matrixgame;
namespace p=platform3g;
std::vector<float> progress;
[[noreturn]] void Unsupported(){throw p::Unsupported("native services ownership fixture does not implement host services");}
SMGDRangersInterface Table(){
 SMGDRangersInterface table{};
 table.m_Sound=[](wchar_t*){Unsupported();};
 table.m_SoundCreate=[](wchar_t*,int,int)->std::uint32_t{Unsupported();};
 table.m_SoundDestroy=[](std::uint32_t){Unsupported();};table.m_SoundPlay=table.m_SoundDestroy;
 table.m_SoundIsPlay=[](std::uint32_t)->int{Unsupported();};
 table.m_SoundVolume=[](std::uint32_t,float){Unsupported();};table.m_SoundPan=table.m_SoundVolume;
 table.m_SoundGetVolume=[](std::uint32_t)->float{Unsupported();};table.m_SoundGetPan=table.m_SoundGetVolume;
 table.m_RangersText=[](wchar_t*,wchar_t*,std::uint32_t,int,int,int,int,int,int,int,Base::CRect*,SMGDRangersInterfaceText*){Unsupported();};
 table.m_RangersTextClear=[](SMGDRangersInterfaceText*){Unsupported();};
 table.m_ProgressBar=[](float value){progress.push_back(value);};
 table.m_Takt=[](){Unsupported();};table.m_Begin=table.m_Takt;
 table.m_MusicVolumeGet=[]()->float{Unsupported();};table.m_MusicVolumeSet=[](float){Unsupported();};
 return table;
}
struct Window final:p::Window {
 unsigned attachments{},unexpected{};
 void* Identity()const noexcept override{return const_cast<Window*>(this);}
 p::WindowRect ReadClientRect()override{return {0,0,1280,720};}
 struct Lease final:p::WindowAttachment {Window& owner;explicit Lease(Window& w):owner(w){++owner.attachments;}void Restore()noexcept override{}~Lease(){--owner.attachments;}};
 std::unique_ptr<p::WindowAttachment> AttachNative()override{return std::make_unique<Lease>(*this);}
 std::unique_ptr<p::WindowAttachment> Attach(p::MessageHandler)override{++unexpected;throw p::Unsupported("no Win32 hook");}
 void Show(p::Visibility)override{++unexpected;throw p::Unsupported("no window display");}
 void ReleaseCursor()override{++unexpected;throw p::Unsupported("no cursor clipping");}
 std::intptr_t DefaultMessage(std::uint32_t,std::uintptr_t,std::intptr_t)override{++unexpected;throw p::Unsupported("no Win32 message");}
};
struct Device final:p::Device {
 unsigned caps{};const SMGDRangersInterface* caller{};bool expected{};
 p::DeviceCaps ReadCaps()override{
  ++caps;assert(g_MatrixHeap && g_MatrixData && output_paths::NativeBound());
  assert(bool(g_RangersInterface)==expected);
  if(expected)assert(g_RangersInterface!=caller && g_RangersInterface->m_ProgressBar==caller->m_ProgressBar);
  throw p::Unsupported("NATIVE_SERVICES_CAPS_PENDING");
 }
 p::GammaRamp ReadGamma(unsigned)override{throw p::Unsupported("no gamma");}
 void RestoreGamma(unsigned,const p::GammaRamp&)noexcept override{std::terminate();}
 std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("no GPU memory");}
};
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
void Clean(){assert(!g_RangersInterface && !g_MatrixHeap && !g_MatrixData && !g_Cache && !g_CacheHeap && !g_MatrixMap && !output_paths::NativeBound());}
int main(){
 Clean();Window window;Device device;auto table=Table();
 {
  NativeRangersServicesScope binding(table);auto* bound=g_RangersInterface;assert(bound && bound!=&table);
  // Copy protects dispatch from subsequent caller table mutation.
  auto original=table.m_ProgressBar;table.m_ProgressBar=[](float){throw std::logic_error("caller mutated table used");};
  CLoadProgress load;load.SetCurLP(0);load.InitCurLP(100);load.SetCurLPPos(50);assert(progress.size()==1 && progress.back()>0);
  Reject([&]{NativeRangersServicesScope duplicate{Table()};});assert(g_RangersInterface==bound);
  table.m_ProgressBar=original;
 }
 Clean();
 {
  auto failing=Table();failing.m_ProgressBar=[](float){throw std::runtime_error("HOST_PROGRESS_FAILURE");};
  bool propagated=false;
  try{NativeRangersServicesScope binding(failing);CLoadProgress load;load.SetCurLP(0);load.InitCurLP(100);load.SetCurLPPos(50);}
  catch(const std::runtime_error& error){propagated=std::string(error.what())=="HOST_PROGRESS_FAILURE";}
  assert(propagated);Clean();
 }
 unsigned prefix{};
 for(bool provide:{false,true}){
  NativeInitRuntime runtime;NativeGameOptions options{window,device};options.seed=37;options.services=provide?&table:nullptr;
#ifdef _WIN32
  options.output_root=L"C:/matrixgame-services-entry-output";
#else
  options.output_root=L"/matrixgame-services-entry-output";
#endif
  device.expected=provide;device.caller=&table;
  bool stopped=false;try{runtime.InitializeNativeGame(options);}catch(const p::Unsupported& error){stopped=std::string(error.what())=="NATIVE_SERVICES_CAPS_PENDING";}
  assert(stopped && !runtime.CpuReady() && !runtime.OriginalGameReady() && !window.attachments && !window.unexpected);Clean();
  runtime.Deinit();Clean();++prefix;
 }
 auto fields=std::make_tuple(&SMGDRangersInterface::m_Sound,&SMGDRangersInterface::m_SoundCreate,&SMGDRangersInterface::m_SoundDestroy,
 &SMGDRangersInterface::m_SoundPlay,&SMGDRangersInterface::m_SoundIsPlay,&SMGDRangersInterface::m_SoundVolume,&SMGDRangersInterface::m_SoundPan,
 &SMGDRangersInterface::m_SoundGetVolume,&SMGDRangersInterface::m_SoundGetPan,&SMGDRangersInterface::m_RangersText,&SMGDRangersInterface::m_RangersTextClear,
 &SMGDRangersInterface::m_ProgressBar,&SMGDRangersInterface::m_Takt,&SMGDRangersInterface::m_Begin,&SMGDRangersInterface::m_MusicVolumeGet,&SMGDRangersInterface::m_MusicVolumeSet);
 unsigned missing{};
 std::apply([&](auto... member){([&]{
  auto incomplete=Table();incomplete.*member=nullptr;NativeInitRuntime runtime;NativeGameOptions options{window,device};options.services=&incomplete;options.output_root=L"";
  bool rejected=false;try{runtime.InitializeNativeGame(options);}catch(const std::invalid_argument& error){rejected=std::string(error.what()).starts_with("missing native Rangers service:");}
  assert(rejected && device.caps==2 && !runtime.CpuReady());Clean();++missing;
 }(),...);},fields);
 // Existing caller-managed binding must survive both rejected takeover and
 // destruction/Deinit of an idle runtime with no owned services scope.
 {
  g_RangersInterface=&table;
  {NativeInitRuntime runtime;NativeGameOptions options{window,device};options.services=&table;options.output_root=L"";
   bool takeover_rejected=false;try{runtime.InitializeNativeGame(options);}catch(const std::logic_error& error){takeover_rejected=std::string(error.what())=="native Rangers services already bound";}
   assert(takeover_rejected);runtime.Deinit();assert(g_RangersInterface==&table && device.caps==2);}
  assert(g_RangersInterface==&table);g_RangersInterface=nullptr;
 }
 Clean();
 std::printf("NATIVE_SERVICES copied_table=PASS original_progress_dispatch=%zu Init_prefix_cycles=%u missing_callbacks_rejected_before_Init=%u takeover_preserved=PASS rollback_unbind=PASS callback_failure_unbind=PASS NO_HOST_AUDIO_TEXT_IMPLEMENTATION NO_GPU NO_SWITCH\n",progress.size(),prefix,missing);
}
