// SPDX-License-Identifier: GPL-2.0-or-later
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#include "matrixgame_actual_draw_audit.hpp"
#include "matrixgame_draw_budget.hpp"
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
namespace matrixgame_draw_audit {
namespace {
struct Counts {std::uint64_t calls{},primary{},indexed{};};
struct Capture {
 std::array<Counts,256> frames{};unsigned count{},errors{};Counts current;bool active{};
};
Capture* capture{};unsigned rejected{};
}
struct Session::Impl {
 srhd_awa::matrixgame::platform3g::D3D9Device& device;Capture data;bool finished{};
 explicit Impl(srhd_awa::matrixgame::platform3g::D3D9Device& value):device(value){
  if(capture)throw std::logic_error("nested frame budget capture");capture=&data;device.ObserveDraws(Observe,this);
 }
 ~Impl(){device.ObserveDraws(nullptr,nullptr);capture=nullptr;}
 static void Observe(void* context,void* raw,std::uint32_t,bool indexed,std::uint32_t,const void*,std::uint32_t)noexcept{
  auto& self=*static_cast<Impl*>(context);if(!self.data.active)return;
  auto& frame=self.data.current;++frame.calls;frame.indexed+=indexed;
  auto& sdk=*static_cast<IDirect3DDevice9*>(raw);IDirect3DSurface9 *target{},*primary{};
  const auto a=sdk.GetRenderTarget(0,&target),b=sdk.GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&primary);
  if(FAILED(a) || FAILED(b))++self.data.errors;else frame.primary+=target==primary;
  if(primary)primary->Release();if(target)target->Release();
 }
 void Finish(){
  if(finished || data.active)throw std::logic_error("unfinished frame budget capture");finished=true;device.ObserveDraws(nullptr,nullptr);
  static unsigned session{};const auto* root=std::getenv("MATRIXGAME_DRAW_AUDIT_OUTPUT");if(!root)throw std::runtime_error("budget output missing");
  std::ofstream output(std::filesystem::path(root)/("frame-cost-"+std::to_string(session++)+".csv"));
  output<<"frame,all_draws,primary_draws,indexed_draws\n";std::uint64_t maximum{},total{};unsigned over{};
  for(unsigned i=0;i<data.count;++i){const auto& f=data.frames[i];maximum=std::max(maximum,f.primary);total+=f.primary;over+=f.primary>2048;
   output<<i<<','<<f.calls<<','<<f.primary<<','<<f.indexed<<'\n';}
  if(!output)throw std::runtime_error("budget capture write failed");
  std::printf("ORIGINAL_FRAME_COST frames=%u primary_draws=%llu max_primary_draws=%llu primary_over_2048=%u errors=%u ACTUAL_HAL NO_NATIVE_GPU\n",data.count,total,maximum,over,data.errors);
  if(!data.count || data.errors || over)++rejected;
 }
};
FrameBudget::FrameBudget(){if(!capture || capture->active)throw std::logic_error("frame budget without session or nested");capture->current={};capture->active=true;}
FrameBudget::~FrameBudget()noexcept{if(!capture || !capture->active)std::terminate();capture->active=false;
 if(capture->count==capture->frames.size()){++capture->errors;return;}capture->frames[capture->count++]=capture->current;}
Session::Session(srhd_awa::matrixgame::platform3g::Device& device):impl_(std::make_unique<Impl>(dynamic_cast<srhd_awa::matrixgame::platform3g::D3D9Device&>(device))){}
Session::~Session()=default;
void Session::Finish(){impl_->Finish();}
int FinalResult(int hal_result)noexcept{
 std::printf("ORIGINAL_FRAME_COST_RESULT original_HAL=%d native_2048_guard_rejected_sessions=%u SOURCE_POLICY_CHECK_ONLY\n",hal_result,rejected);
 return hal_result?hal_result:rejected?10:0;
}
}
