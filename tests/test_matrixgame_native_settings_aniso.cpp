// SPDX-License-Identifier: GPL-2.0-or-later
// Original manager calls the actual native sampler state; no GPU/caps success.
#include "MatrixSampleStateManager.hpp"
#include "native_game_settings.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#ifdef MATRIXGAME_SETTINGS_AF_CANDIDATE
#include "sampler_deko.hpp"
#endif
namespace m=srhd_awa::matrixgame;namespace p=m::platform3g;
struct Device final:p::Device {
 std::array<m::MeshSamplerState,8> samplers;unsigned failed_register{};
 p::DeviceResult SetSamplerState(unsigned slot,m::SamplerRegister reg,unsigned value)override{
  if(unsigned(reg)==failed_register)return static_cast<std::int32_t>(0x88760868u);
  samplers.at(slot).Apply(reg,value);return 0;
 }
 p::DeviceCaps ReadCaps()override{throw p::Unsupported("AF component has no GPU caps");}
 std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("AF component has no GPU memory");}
 p::GammaRamp ReadGamma(unsigned)override{throw p::Unsupported("AF component has no gamma");}
 void RestoreGamma(unsigned,const p::GammaRamp&)noexcept override{std::terminate();}
};
template<class F>void Reject(F&& f){bool rejected=false;try{f();}catch(const std::invalid_argument&){rejected=true;}assert(rejected);}
int main(int argc,char** argv){
 Device device;assert(!p::Current().ready);const auto previous_caps=p::Current().caps;auto* previous_device=p::Current().device;
 auto restore=[&]{p::Current().ready=false;p::Current().device=previous_device;p::Current().caps=previous_caps;};
 p::Current().device=&device;p::Current().ready=true;p::Current().caps.max_anisotropy=16;
 if(argc==2 && !std::strcmp(argv[1],"--baseline")){
  auto settings=SRobotsSettings::generate_default_settings();settings.m_AFDegree=4;CMatrixSampleStateManager manager;manager.ApplySettings(&settings);
  for(auto reg:{m::SamplerRegister::MagFilter,m::SamplerRegister::MipFilter}){
   const auto before=device.samplers[0];Reject([&]{manager.SetState(0,reg,2);});assert(before==device.samplers[0]);}
  restore();std::puts("BASELINE original AF manager promoted MAG/MIP linear requests to unsupported native ANISOTROPIC: REPRODUCED");return 0;
 }
#ifdef MATRIXGAME_SETTINGS_AF_CANDIDATE
 unsigned cases{},exact_sdk{};
 for(unsigned cap:{0u,1u,4u,16u})for(unsigned degree:{0u,1u,4u,16u,24u})for(unsigned slot=0;slot<8;++slot){
  device.samplers[slot]={};p::Current().caps.max_anisotropy=cap;
  auto caller=SRobotsSettings::generate_default_settings();caller.m_AFDegree=degree;auto settings=m::PrepareNativeGameSettings(&caller,{0,0,1280,720});
  CMatrixSampleStateManager manager;manager.ApplySettings(&settings);const auto expected=std::min(cap,degree);
  assert(manager.SetState(slot,m::SamplerRegister::MagFilter,2)==0 && manager.SetState(slot,m::SamplerRegister::MipFilter,2)==0 && manager.SetState(slot,m::SamplerRegister::MinFilter,2)==0);
  auto& state=device.samplers[slot];assert(state.mag==m::TextureFilter::Linear && state.mip==m::TextureMipFilter::Linear);
  assert(state.min==(expected?m::TextureFilter::Anisotropic:m::TextureFilter::Linear));assert(state.max_anisotropy==(expected?expected:1));
  const auto sdk=m::MakeDekoSampler(state);
  assert(sdk.minFilter==DkFilter_Linear && sdk.magFilter==DkFilter_Linear && sdk.mipFilter==DkMipFilter_Linear && sdk.maxAnisotropy==float(expected?expected:1));++exact_sdk;
  // The same SDK fields as the previous inline Record construction.
  auto filter=[](m::TextureFilter f){return f==m::TextureFilter::Point?DkFilter_Nearest:DkFilter_Linear;};
  auto wrap=[](m::TextureAddress a){return a==m::TextureAddress::Clamp?DkWrapMode_ClampToEdge:DkWrapMode_Repeat;};
  dk::Sampler old;old.setFilter(filter(state.min),filter(state.mag),state.mip==m::TextureMipFilter::None?DkMipFilter_None:state.mip==m::TextureMipFilter::Point?DkMipFilter_Nearest:DkMipFilter_Linear)
   .setWrapMode(wrap(state.u),wrap(state.v),wrap(state.w)).setLodBias(state.lod_bias).setMaxAnisotropy(state.min==m::TextureFilter::Anisotropic?float(state.max_anisotropy):1.f);
  assert(!std::memcmp(&old,&sdk,sizeof(sdk)));
  assert(manager.SetState(slot,m::SamplerRegister::MinFilter,1)==0);const auto point=m::MakeDekoSampler(state);assert(point.minFilter==DkFilter_Nearest && point.maxAnisotropy==1.f);
  const auto before=state;Reject([&]{state.Apply(m::SamplerRegister::MagFilter,3);});Reject([&]{state.Apply(m::SamplerRegister::MipFilter,3);});assert(state==before);
  assert(caller.m_AFDegree==int(degree) && settings.m_AFDegree==int(degree));++cases;
 }
 auto settings=SRobotsSettings::generate_default_settings();settings.m_AFDegree=4;p::Current().caps.max_anisotropy=16;CMatrixSampleStateManager manager;manager.ApplySettings(&settings);
 device.failed_register=10;assert(manager.SetState(0,m::SamplerRegister::MinFilter,2)==static_cast<std::int32_t>(0x88760868u));
 restore();
 std::printf("native original AF settings cases=%u actual_SDK_samplers=%u min_anisotropy=REAL mag_mip_original_requests=LINEAR point_restored=PASS raw_invalid_filters_rejected=PASS error_propagation=PASS CPU_SDK_FIELDS_ONLY NO_GPU\n",cases,exact_sdk);
#endif
}
