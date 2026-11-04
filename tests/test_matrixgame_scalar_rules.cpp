// Executed separately against frozen r11 and candidate production .cpp.
// Compare valid values, exception categories/messages and corrupted state.
#ifdef NDEBUG
#error State equivalence checks require assertions
#endif
#include "native_device_state.hpp"
#include "native_state_layout.hpp"
#include <cassert>
#include <cstdio>
#include <cfenv>
#include <bit>
#include <cstring>
namespace p=srhd_awa::matrixgame::platform3g;
std::uint64_t hash=1469598103934665603ull,cases{},rejections{};
void Mix(std::uint64_t v){hash=(hash^v)*1099511628211ull;}
template<class F>void Observe(F f){Mix(++cases);try{auto v=f();Mix(1);Mix(v);}catch(const std::exception& e){++rejections;Mix(0);
 Mix(dynamic_cast<const p::Unsupported*>(&e)?1:dynamic_cast<const std::out_of_range*>(&e)?2:dynamic_cast<const std::invalid_argument*>(&e)?3:4);
 for(const char* s=e.what();*s;++s)Mix(static_cast<unsigned char>(*s));}}
p::NativeRenderState Baseline(){p::NativeRenderState s;s.vertex.Layout(0x112);s.vertex.Viewport({0,0,1280,720,0,1});for(unsigned r:{1u,4u})s.combiner.Set(0,r,2);for(unsigned r:{2u,5u})s.combiner.Set(0,r,0);return s;}
int Legacy(){
 const auto base=Baseline();Observe([&]{base.ValidateDraw();return 0u;});
 for(unsigned id=0;id<512;++id)Observe([&]{return base.Render(id);});
 for(const auto& w:p::NativeRenderWords){
  for(unsigned value:{0u,1u,2u,3u,4u,0xffffffffu,0x7fc00000u,w.initial}){
   Observe([&]{auto s=base;s.Render(w.id,value);return s.Render(w.id);});
   Observe([&]{auto s=base;s.Render(w.id,value);s.ValidateDraw();return s.Render(w.id);});
   Observe([&]{auto s=base;s.registers.at(w.id)=value;s.ValidateDraw();return s.Render(w.id);});
  }
  // Even an inactive missing word must keep its original failure behavior.
  Observe([&]{auto s=base;s.registers.erase(w.id);s.ValidateDraw();return 0u;});
  Observe([&]{auto s=base;s.registers.erase(w.id);return s.Render(w.id);});
 }
 for(bool sampled:{false,true})for(unsigned slot=0;slot<8;++slot)for(unsigned reg=1;reg<=13;++reg)for(unsigned value:{0u,1u,2u,0xffffffffu}){
  Observe([&]{auto s=base;if(sampled){s.combiner.Bind(slot,true);s.combiner.Set(slot,1,2);s.combiner.Set(slot,2,2);s.combiner.Set(slot,4,2);s.combiner.Set(slot,5,2);}s.Sampler(slot,reg,value);s.ValidateDraw();return s.Sampler(slot,reg);});
 }
 for(unsigned samples:{0u,1u})for(unsigned quality:{0u,1u})for(bool points:{false,true})for(bool lines:{false,true})
  Observe([&]{base.ValidateDraw(points,lines,samples,quality);return 0u;});
 assert(cases==4039 && rejections>1000);
 std::printf("STATE_LOOKUP cases=%llu rejected=%llu checksum=%llu all_103_words unknown_ids setters_getters_missing_inactive_words sampled_samplers draw_modes CPU_ONLY\n",(unsigned long long)cases,(unsigned long long)rejections,(unsigned long long)hash);
 return 0;
}

void Bytes(const void* ptr,std::size_t size){for(std::size_t i=0;i<size;++i)Mix(static_cast<const unsigned char*>(ptr)[i]);}
void Snapshot(const p::NativeRenderState& state){
 Mix(state.cull);Mix(state.shade);Mix(state.registers.size());for(const auto& [id,v]:state.registers){Mix(id);Mix(v);}
 for(const auto& w:p::NativeRenderWords){Mix(w.id);try{Mix(state.Render(w.id));Mix(1);}catch(const std::exception& e){Mix(0);for(const char* c=e.what();*c;++c)Mix(static_cast<unsigned char>(*c));}}
 const auto raster=state.raster.Words();for(auto v:raster)Mix(v);
 Bytes(&state.combiner.State(),sizeof(state.combiner.State()));for(bool v:state.combiner.Bound())Mix(v);
 for(const auto& sampler:state.samplers)for(auto v:sampler.Words())Mix(v);for(const auto& extra:state.sampler_extra)for(auto v:extra)Mix(v);
 const auto vertex=state.vertex.Prepare();Bytes(&vertex,sizeof(vertex));
}
int main(){assert(Legacy()==0);const auto base=Baseline();unsigned setters=0;const int original=std::fegetround();
 for(int mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){assert(std::fesetround(mode)==0);
  for(const auto& w:p::NativeRenderWords)for(unsigned value:{0u,1u,2u,3u,4u,5u,7u,8u,9u,15u,16u,17u,18u,255u,256u,0x80000000u,1u,0x00800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc12345u,0x7f812345u,0xffffffffu,w.initial})for(bool erased:{false,true}){
   auto state=base;if(erased)state.registers.erase(w.id);
   Observe([&]{std::feclearexcept(FE_ALL_EXCEPT);try{state.Render(w.id,value);}catch(...){const int flags=std::fetestexcept(FE_ALL_EXCEPT);std::feclearexcept(FE_ALL_EXCEPT);Mix(flags);Snapshot(state);std::feclearexcept(FE_ALL_EXCEPT);throw;}
    const int flags=std::fetestexcept(FE_ALL_EXCEPT);std::feclearexcept(FE_ALL_EXCEPT);Mix(flags);Snapshot(state);std::feclearexcept(FE_ALL_EXCEPT);state.ValidateDraw();return state.Render(w.id);
   });++setters;
  }
  for(unsigned id:{0u,1u,511u,0xffffffffu})for(unsigned value:{0u,1u,0x7f812345u}){
   auto state=base;state.registers[id]=0xdec0ad;
   Observe([&]{try{state.Render(id,value);}catch(...){Snapshot(state);throw;}assert(false);return 0u;});assert(state.registers.at(id)==0xdec0ad);++setters;
  }
 }
 assert(std::fesetround(original)==0);std::feclearexcept(FE_ALL_EXCEPT);
 std::printf("SCALAR_RULES cases=%llu rejected=%llu checksum=%llu setter_cases=%u full_state_after_success_failure missing_map_priority all103 rounding4_FPflags CPU_ONLY_NOT_GPU\n",(unsigned long long)cases,(unsigned long long)rejections,(unsigned long long)hash,setters);
}
