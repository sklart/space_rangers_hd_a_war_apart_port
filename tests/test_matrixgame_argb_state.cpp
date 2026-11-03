// Full CPU draw preparation against frozen r15 and candidate production CPP.
// Compare valid values, exception categories/messages and corrupted state.
#ifdef NDEBUG
#error State equivalence checks require assertions
#endif
#include "matrixgame_draw_preparation_fixture.hpp"
#include "native_state_layout.hpp"
#include <cassert>
#include <cstdio>
#include <cfenv>
namespace p=srhd_awa::matrixgame::platform3g;
std::uint64_t hash=1469598103934665603ull,cases{},rejections{};
void Mix(std::uint64_t v){hash=(hash^v)*1099511628211ull;}
template<class F>void Observe(F f){Mix(++cases);try{auto v=f();Mix(1);Mix(v);}catch(const std::exception& e){++rejections;Mix(0);
 Mix(dynamic_cast<const p::Unsupported*>(&e)?1:dynamic_cast<const std::out_of_range*>(&e)?2:dynamic_cast<const std::invalid_argument*>(&e)?3:4);
 for(const char* s=e.what();*s;++s)Mix(static_cast<unsigned char>(*s));}}
void Validate(const p::NativeRenderState& s,bool points=false,bool lines=false,unsigned samples=0,unsigned quality=0){
 for(bool alpha_target:{false,true}){
 const auto d=Prepare(s,points,lines,samples,quality,alpha_target);for(auto b:d.packet)Mix(std::to_integer<unsigned>(b));
 const auto& t=d.stencil;Mix(t.enabled);Mix(t.two_sided);Mix(t.reference);Mix(t.read_mask);Mix(t.write_mask);
 for(auto f:{t.clockwise,t.counterclockwise}){Mix(f.fail);Mix(f.depth_fail);Mix(f.pass);Mix(f.compare);}
 }
}
void ColorRoundtrip(const p::NativeRenderState& s,unsigned color){
 assert(std::feclearexcept(FE_ALL_EXCEPT)==0);const auto factor=s.Render(60);const auto factor_flags=std::fetestexcept(FE_ALL_EXCEPT);assert(factor==color);Mix(factor_flags);
 assert(std::feclearexcept(FE_ALL_EXCEPT)==0);const auto ambient=s.Render(139);const auto ambient_flags=std::fetestexcept(FE_ALL_EXCEPT);assert(ambient==color);Mix(ambient_flags);
}
p::NativeRenderState Baseline(){p::NativeRenderState s;s.vertex.Layout(0x112);s.vertex.Viewport({0,0,1280,720,0,1});for(unsigned r:{1u,4u})s.combiner.Set(0,r,2);for(unsigned r:{2u,5u})s.combiner.Set(0,r,0);return s;}
int main(){
 const auto base=Baseline();Observe([&]{Validate(base);return 0u;});
 for(unsigned id=0;id<512;++id)Observe([&]{return base.Render(id);});
 for(const auto& w:p::NativeRenderWords){
  for(unsigned value:{0u,1u,2u,3u,4u,0xffffffffu,0x7fc00000u,w.initial}){
   Observe([&]{auto s=base;s.Render(w.id,value);return s.Render(w.id);});
   Observe([&]{auto s=base;s.Render(w.id,value);Validate(s);return s.Render(w.id);});
   Observe([&]{auto s=base;s.registers.at(w.id)=value;Validate(s);return s.Render(w.id);});
  }
  // Even an inactive missing word must keep its original failure behavior.
  Observe([&]{auto s=base;s.registers.erase(w.id);Validate(s);return 0u;});
  Observe([&]{auto s=base;s.registers.erase(w.id);return s.Render(w.id);});
 }
 for(bool sampled:{false,true})for(unsigned slot=0;slot<8;++slot)for(unsigned reg=1;reg<=13;++reg)for(unsigned value:{0u,1u,2u,0xffffffffu}){
  Observe([&]{auto s=base;if(sampled){s.combiner.Bind(slot,true);s.combiner.Set(slot,1,2);s.combiner.Set(slot,2,2);s.combiner.Set(slot,4,2);s.combiner.Set(slot,5,2);}s.Sampler(slot,reg,value);Validate(s);return s.Sampler(slot,reg);});
 }
 for(unsigned samples:{0u,1u})for(unsigned quality:{0u,1u})for(bool points:{false,true})for(bool lines:{false,true})
  Observe([&]{Validate(base,points,lines,samples,quality);return 0u;});
 // Successive changes on one state must never reuse a previous draw value.
 auto changing=base;
 for(unsigned i=0;i<128;++i)Observe([&]{
  changing.Render(28,i%2);changing.Render(35,1+i%3);changing.Render(36,0);changing.Render(37,0x3f800000);changing.Render(38,0x3f000000);changing.Render(34,0xff000000u+i*12345);
  changing.Render(52,i%2);changing.Render(57,i);changing.Render(185,(i/2)%2);changing.Render(53,1+i%8);changing.Render(186,1+(i/8)%8);
  changing.combiner.Factor(0xff000000u+i*789);changing.shade=1+i%2;
  Validate(changing);return i;
 });
 // Two simultaneous invalid fields retain the first exception from r15.
 for(unsigned id:{28u,35u,52u,53u,16u,176u})Observe([&]{auto s=base;s.registers.at(id)=0xffffffffu;s.registers.at(26)=1;Validate(s);return 0u;});
 // Extraneous keys before/between/after metadata IDs must not perturb the walk.
 for(unsigned id:{0u,1u,6u,10u,30u,210u,256u,0xffffffffu})for(unsigned value:{0u,1u,0xffffffffu})
  Observe([&]{auto s=base;s.registers.emplace(id,value);Validate(s);return 0u;});
 // Preflight failures must retain priority over other missing raw words.
 for(unsigned id:{28u,35u,36u,37u,38u,34u,52u,185u,53u,54u,55u,56u,186u,187u,188u,189u,57u,58u,59u,174u})for(bool scissor:{false,true})
  Observe([&]{auto s=base;s.registers.at(174)=scissor;s.registers.erase(id);Validate(s);return 0u;});
 for(bool scissor:{false,true})for(unsigned samples:{0u,1u})for(unsigned quality:{0u,1u})
  Observe([&]{auto s=base;s.registers.clear();s.registers.emplace(174,scissor);Validate(s,false,false,samples,quality);return 0u;});
 // Independent ARGB8 roundtrip oracle, all channels and four FE rounding modes.
 for(int mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){
  assert(std::fesetround(mode)==0);
  for(unsigned shift:{0u,8u,16u,24u})for(unsigned value=0;value<256;++value)
   Observe([&]{auto s=base;const unsigned color=(0xa15c3e97u&~(255u<<shift))|(value<<shift);
    s.Render(60,color);s.Render(139,color);ColorRoundtrip(s,color);Validate(s);return color;});
 }
 assert(std::fesetround(FE_TONEAREST)==0);
 const std::array<unsigned,16> boundary{0u,0xffffffffu,0xff000000u,0x00ffffffu,0x01010101u,0x80808080u,0x7f7f7f7fu,0xfefefefeu,0xffu,0xff00u,0xff0000u,0xff000000u,0x55555555u,0xaaaaaaaau,0x12345678u,0xdeadbeefu};
 const std::array<int,4> modes{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
 unsigned rng=0x12345678u;
 for(unsigned i=0;i<4096;++i)Observe([&]{
  if(i<64)assert(std::fesetround(modes[i/16])==0);else if(i==64)assert(std::fesetround(FE_TONEAREST)==0);
  rng=rng*1664525u+1013904223u;const auto color=i<64?boundary[i%16]:rng;
  auto s=base;s.Render(60,color);s.Render(139,color);ColorRoundtrip(s,color);Validate(s);return color;
 });
 assert(cases==12437 && rejections>1000);
 std::printf("ARGB_STATE cases=%llu rejected=%llu checksum=%llu all_103_words unknown_ids setters_getters_missing_inactive_words sampled_samplers draw_modes CPU_ONLY\n",(unsigned long long)cases,(unsigned long long)rejections,(unsigned long long)hash);
}
