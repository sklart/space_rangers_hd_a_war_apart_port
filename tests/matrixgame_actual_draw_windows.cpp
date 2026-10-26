// SPDX-License-Identifier: GPL-2.0-or-later
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#include "matrixgame_actual_draw_audit.hpp"
#include "matrixgame_actual_draw_state.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace matrixgame_draw_audit {
namespace {
unsigned rejected_sessions{};
void Check(HRESULT r){p::CheckDeviceResult(r,"draw audit actual SDK query");}
template<class T>struct Com {T* value{};~Com(){if(value)value->Release();}};
struct Less{bool operator()(const State& a,const State& b)const noexcept{return std::memcmp(&a,&b,sizeof(a))<0;}};
State Read(IDirect3DDevice9& d,unsigned topology,bool indexed){
 State s{};s.topology=topology;s.indexed=indexed;DWORD value{};Check(d.GetFVF(&value));s.fvf=value;
 static_assert(sizeof(D3DVIEWPORT9)==sizeof(s.viewport));D3DVIEWPORT9 viewport{};Check(d.GetViewport(&viewport));std::memcpy(&s.viewport,&viewport,sizeof(viewport));
 static_assert(sizeof(D3DMATERIAL9)==sizeof(s.material));D3DMATERIAL9 material{};Check(d.GetMaterial(&material));std::memcpy(&s.material,&material,sizeof(material));
 for(unsigned i=0;i<p::NativeRenderWords.size();++i){Check(d.GetRenderState(static_cast<D3DRENDERSTATETYPE>(p::NativeRenderWords[i].id),&value));s.render[i]=value;}
 Com<IDirect3DSurface9> target;Check(d.GetRenderTarget(0,&target.value));D3DSURFACE_DESC desc{};Check(target.value->GetDesc(&desc));s.multisample=desc.MultiSampleType;
 Com<IDirect3DTexture9> target_texture;const auto container=target.value->GetContainer(__uuidof(IDirect3DTexture9),reinterpret_cast<void**>(&target_texture.value));
 if(FAILED(container) && container!=E_NOINTERFACE)Check(container);
 Com<IDirect3DSurface9> depth;const auto depth_result=d.GetDepthStencilSurface(&depth.value);
 if(SUCCEEDED(depth_result)){Check(depth.value->GetDesc(&desc));s.depth_format=desc.Format;}else if(depth_result!=D3DERR_NOTFOUND)Check(depth_result);
 for(unsigned slot=0;slot<8;++slot){
  for(unsigned i=0;i<13;++i){Check(d.GetSamplerState(slot,static_cast<D3DSAMPLERSTATETYPE>(i+1),&value));s.sampler[slot][i]=value;}
  for(unsigned i=0;i<StageKeys.size();++i){Check(d.GetTextureStageState(slot,static_cast<D3DTEXTURESTAGESTATETYPE>(StageKeys[i]),&value));s.stage[slot][i]=value;}
  Com<IDirect3DBaseTexture9> texture;Check(d.GetTexture(slot,&texture.value));
  if(texture.value)s.bound|=1u<<slot;
  if(texture.value && texture.value==target_texture.value)s.feedback=1;
  D3DLIGHT9 light{};BOOL enabled{};const auto lr=d.GetLight(slot,&light),er=d.GetLightEnable(slot,&enabled);
  if(lr==D3DERR_INVALIDCALL && er==D3DERR_INVALIDCALL)continue;
  Check(lr);Check(er);static_assert(sizeof(light)==sizeof(s.lights[slot]));std::memcpy(&s.lights[slot],&light,sizeof(light));s.light_present|=1u<<slot;s.light_enabled[slot]=enabled;
 }
 for(unsigned i=0;i<TransformKeys.size();++i){D3DMATRIX m{};Check(d.GetTransform(static_cast<D3DTRANSFORMSTATETYPE>(TransformKeys[i]),&m));std::memcpy(s.transform[i].data(),&m,sizeof(m));}
 return s;
}
}
struct Session::Impl {
 struct Line {State state;std::uint32_t primitives,stride,width,height,format;std::vector<std::byte> bytes;};
 std::vector<Line> lines;
 p::D3D9Device& device;std::map<State,std::uint64_t,Less> states;std::map<std::string,std::uint64_t> failures;
 std::uint64_t draws{},indexed{};bool finished{};
 explicit Impl(p::D3D9Device& d):device(d){device.ObserveDraws(Observe,this);}
 ~Impl(){device.ObserveDraws(nullptr,nullptr);}
 static void Observe(void* context,void* sdk,std::uint32_t topology,bool indices,std::uint32_t primitives,const void* immediate,std::uint32_t stride)noexcept{
  auto& self=*static_cast<Impl*>(context);++self.draws;self.indexed+=indices;
  try{auto& device=*static_cast<IDirect3DDevice9*>(sdk);auto state=Read(device,topology,indices);auto [it,inserted]=self.states.try_emplace(state,0);++it->second;
   if(topology==2 || topology==3){
    if(!immediate || !stride || indices || primitives>10000)throw std::runtime_error("actual line capture requires bounded UP source");
    const auto elements=topology==2?std::uint64_t(primitives)*2:std::uint64_t(primitives)+1;
    if(elements*stride>1024*1024)throw std::runtime_error("actual line capture byte limit");
    Com<IDirect3DSurface9> target;Check(device.GetRenderTarget(0,&target.value));D3DSURFACE_DESC desc{};Check(target.value->GetDesc(&desc));
    const auto* bytes=static_cast<const std::byte*>(immediate);self.lines.push_back({state,primitives,stride,desc.Width,desc.Height,static_cast<std::uint32_t>(desc.Format),{bytes,bytes+elements*stride}});
   }
   if(inserted)try{Validate(state);}catch(const std::exception& e){++self.failures[e.what()];
    void* stack[16]{};const auto count=CaptureStackBackTrace(1,16,stack,nullptr);
    std::printf("NATIVE_REJECT_CALLER topology=%u fvf=%08x lastpixel=%u image=%p stack:",topology,state.fvf,state.render[5],static_cast<void*>(GetModuleHandleW(nullptr)));
    for(USHORT i=0;i<count;++i)std::printf(" %p",stack[i]);std::puts("");}}
  catch(const std::exception& e){try{++self.failures[std::string("SDK capture: ")+e.what()];}catch(...){std::terminate();}}
 }
 void Finish(){
  if(finished)throw std::logic_error("draw audit already finished");finished=true;device.ObserveDraws(nullptr,nullptr);
  static unsigned session;const auto* dir=std::getenv("MATRIXGAME_DRAW_AUDIT_OUTPUT");if(!dir)throw std::runtime_error("draw audit output root missing");
  const auto index=session++;const auto path=std::filesystem::path(dir)/("draw-states-"+std::to_string(index)+".bin");
  std::ofstream file(path,std::ios::binary);const std::array<std::uint32_t,4> header{Magic,1,sizeof(State),static_cast<std::uint32_t>(states.size())};file.write(reinterpret_cast<const char*>(header.data()),sizeof(header));
  for(const auto& [state,count]:states){file.write(reinterpret_cast<const char*>(&state),sizeof(state));file.write(reinterpret_cast<const char*>(&count),sizeof(count));}
  if(!file)throw std::runtime_error("draw audit state write failed");file.close();
  std::ofstream linefile(std::filesystem::path(dir)/("line-geometry-"+std::to_string(index)+".bin"),std::ios::binary);
  const std::array<std::uint32_t,4> lineheader{0x374c524d,1,sizeof(State),static_cast<std::uint32_t>(lines.size())};linefile.write(reinterpret_cast<const char*>(lineheader.data()),sizeof(lineheader));
  for(const auto& line:lines){linefile.write(reinterpret_cast<const char*>(&line.state),sizeof(State));const std::array<std::uint32_t,6> info{line.primitives,line.stride,line.width,line.height,line.format,static_cast<std::uint32_t>(line.bytes.size())};linefile.write(reinterpret_cast<const char*>(info.data()),sizeof(info));linefile.write(reinterpret_cast<const char*>(line.bytes.data()),line.bytes.size());}
  if(!linefile)throw std::runtime_error("actual line geometry write failed");linefile.close();
  std::printf("ACTUAL_MAP_NATIVE_STATE draws=%llu indexed=%llu unique=%zu failures=%zu CPU_VALIDATION NO_DEKO_GPU\n",draws,indexed,states.size(),failures.size());
  for(const auto& [error,count]:failures)std::printf("NATIVE_STATE_REJECT unique=%llu reason=%s\n",count,error.c_str());
  if(!draws || !indexed || states.empty() || !failures.empty())++rejected_sessions;
 }
};
Session::Session(p::Device& device):impl_(std::make_unique<Impl>(dynamic_cast<p::D3D9Device&>(device))){}
Session::~Session()=default;
void Session::Finish(){impl_->Finish();}
int FinalResult(int hal_result)noexcept{
 std::printf("ACTUAL_MAP_HAL_RESULT=%d NATIVE_STATE_REJECTED_SESSIONS=%u\n",hal_result,rejected_sessions);
 return hal_result?hal_result:rejected_sessions?10:0;
}
}
