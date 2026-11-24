#include "native_descriptor_cache.hpp"
#include "sampler_state.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
using namespace srhd_awa::matrixgame;
using srhd_awa::matrixgame::platform3g::NativeDescriptorCache;
int main(){
 NativeDescriptorCache<std::array<std::uint32_t,8>,std::array<std::uint32_t,8>,8> cache;
 MeshSamplerState state;unsigned calls=0;bool hit=false;
 auto get=[&](unsigned slot){return cache.Get(slot,state.Words(),[&]{state.Validate();++calls;return state.Words();},hit);};
 auto original=get(0);assert(!hit && calls==1);
 for(unsigned i=0;i<1000;++i){assert(get(0)==original && hit && calls==1);}
 for(unsigned slot=1;slot<8;++slot){assert(get(slot)==original && hit && calls==1);}
 for(auto reg:SamplerRegisters){
  MeshSamplerState changed;
  const auto value=reg==SamplerRegister::LodBias?0x3f000000u:reg==SamplerRegister::MaxAnisotropy?2u:
                   reg==SamplerRegister::AddressU||reg==SamplerRegister::AddressV||reg==SamplerRegister::AddressW?3u:
                   reg==SamplerRegister::MipFilter?1u:2u;
  changed.Apply(reg,value);state=changed;assert(get(0)==state.Words() && !hit);
 }
 state=MeshSamplerState{};assert(get(1)==original && hit);auto before=calls;
 auto bad=state.Words();bad[0]=99;
 try{cache.Get(1,bad,[&]()->std::array<std::uint32_t,8>{throw std::runtime_error("factory");},hit);assert(false);}catch(const std::runtime_error&){}
 assert(get(1)==original && hit && calls==before);
 try{cache.Get(8,original,[&]{return original;},hit);assert(false);}catch(const std::out_of_range&){}
 std::puts("descriptor key changes, reuse, slot isolation, factory failure PASS; CPU ONLY");
}
