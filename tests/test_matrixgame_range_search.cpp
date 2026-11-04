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
int Scalar(){assert(Legacy()==0);const auto base=Baseline();unsigned setters=0;const int original=std::fegetround();
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
 return 0;
}

int SamplerLegacy(){assert(Scalar()==0);const auto base=Baseline();const int original=std::fegetround();unsigned operations=0;
 for(int mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){assert(std::fesetround(mode)==0);
  for(unsigned slot:{0u,1u,2u,3u,4u,5u,6u,7u,8u,0xffffffffu})for(unsigned id:{0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,255u,0xffffffffu})for(unsigned corrupt:{0u,1u,2u,3u}){
   auto seed=base;for(auto& sampler:seed.samplers){if(corrupt==1)sampler.max_anisotropy=0;if(corrupt==2)sampler.lod_bias=std::bit_cast<float>(0x7f812345u);if(corrupt==3)sampler.u=static_cast<srhd_awa::matrixgame::TextureAddress>(0xffffffffu);}
   Observe([&]{std::feclearexcept(FE_ALL_EXCEPT);try{auto value=seed.Sampler(slot,id);Mix(std::fetestexcept(FE_ALL_EXCEPT));std::feclearexcept(FE_ALL_EXCEPT);Snapshot(seed);return value;}catch(...){Mix(std::fetestexcept(FE_ALL_EXCEPT));std::feclearexcept(FE_ALL_EXCEPT);Snapshot(seed);throw;}});++operations;
   for(unsigned value:{0u,1u,2u,3u,4u,15u,16u,17u,0x80000000u,0x3f800000u,0x7f800000u,0x7fc12345u,0x7f812345u,0xffffffffu}){
    auto state=seed;
    Observe([&]{std::feclearexcept(FE_ALL_EXCEPT);try{state.Sampler(slot,id,value);}catch(...){Mix(std::fetestexcept(FE_ALL_EXCEPT));std::feclearexcept(FE_ALL_EXCEPT);Snapshot(state);throw;}
     Mix(std::fetestexcept(FE_ALL_EXCEPT));std::feclearexcept(FE_ALL_EXCEPT);Snapshot(state);return state.Sampler(slot,id);});++operations;
   }
  }
 }
 assert(std::fesetround(original)==0);std::feclearexcept(FE_ALL_EXCEPT);assert(operations==40800);
 std::printf("SAMPLER_LOOKUP cases=%llu rejected=%llu checksum=%llu sampler_operations=%u full_state_after_success_failure slot_first unknown_before_validation corrupt_sNaN all8slots rounding4_FPflags CPU_ONLY_NOT_GPU\n",(unsigned long long)cases,(unsigned long long)rejections,(unsigned long long)hash,operations);
 return 0;
}


namespace m=srhd_awa::matrixgame;
int RasterLegacy(){assert(SamplerLegacy()==0);const auto base=Baseline();const int original=std::fegetround();unsigned getter_states=0;
 for(int mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){assert(std::fesetround(mode)==0);
  for(unsigned pattern=0;pattern<16;++pattern)for(unsigned field=0;field<7;++field)
   for(unsigned value:{0u,1u,2u,3u,4u,5u,7u,8u,9u,10u,11u,12u,15u,16u,255u,256u,0x80000000u,0x7fc12345u,0x7f812345u,0xffffffffu})for(bool erased:{false,true}){
    auto state=base;auto& raster=state.raster;raster.depth_test=pattern&1;raster.depth_write=pattern&2;raster.alpha_test=pattern&4;raster.blend_enable=pattern&8;
    switch(field){case 0:raster.depth_compare=static_cast<m::RasterCompare>(value);break;case 1:raster.alpha_compare=static_cast<m::RasterCompare>(value);break;
     case 2:raster.alpha_reference=value;break;case 3:raster.color_write=value;break;case 4:raster.source=static_cast<m::RasterBlendFactor>(value);break;
     case 5:raster.destination=static_cast<m::RasterBlendFactor>(value);break;case 6:raster.operation=static_cast<m::RasterBlendOperation>(value);break;}
    if(erased)for(auto id:m::RasterRegisters)state.registers.erase(unsigned(id));
    Observe([&]{std::feclearexcept(FE_ALL_EXCEPT);for(auto id:m::RasterRegisters)Mix(state.Render(unsigned(id)));Mix(std::fetestexcept(FE_ALL_EXCEPT));std::feclearexcept(FE_ALL_EXCEPT);Snapshot(state);return 0u;});++getter_states;
   }
 }
 assert(std::fesetround(original)==0);std::feclearexcept(FE_ALL_EXCEPT);assert(getter_states==17920);
 std::printf("RASTER_READ cases=%llu rejected=%llu checksum=%llu getter_states=%u raster_reads=197120 all11_IDs corrupt_raw_fields bool16 missing_map rounding4_FPflags full_state CPU_ONLY_NOT_GPU\n",(unsigned long long)cases,(unsigned long long)rejections,(unsigned long long)hash,getter_states);
 return 0;
}

void StencilProbe(const m::MeshStencilState&);
void StencilSnapshot(const m::MeshStencilState& s){Mix(s.enabled);Mix(s.two_sided);for(auto f:{s.clockwise,s.counterclockwise})for(auto v:{f.fail,f.depth_fail,f.pass,f.compare})Mix(v);Mix(s.reference);Mix(s.read_mask);Mix(s.write_mask);}
int StencilLegacy(){assert(RasterLegacy()==0);const int original=std::fegetround();unsigned probes=0;
 for(int mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){assert(std::fesetround(mode)==0);
  for(unsigned pattern=0;pattern<4;++pattern)for(unsigned field=0;field<8;++field)
   for(unsigned value:{0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,15u,16u,255u,256u,0x80000000u,0x7f812345u,0xffffffffu}){
    m::MeshStencilState state;state.enabled=pattern&1;state.two_sided=pattern&2;state.clockwise={1,2,3,4};state.counterclockwise={5,6,7,8};state.reference=0xffffffffu;state.read_mask=0x7f812345u;state.write_mask=0x80000000u;
    std::uint32_t* fields[]{&state.clockwise.fail,&state.clockwise.depth_fail,&state.clockwise.pass,&state.clockwise.compare,&state.counterclockwise.fail,&state.counterclockwise.depth_fail,&state.counterclockwise.pass,&state.counterclockwise.compare};*fields[field]=value;const auto before=state;
    Observe([&]{std::feclearexcept(FE_ALL_EXCEPT);try{StencilProbe(state);}catch(...){Mix(std::fetestexcept(FE_ALL_EXCEPT));std::feclearexcept(FE_ALL_EXCEPT);assert(state==before);StencilSnapshot(state);throw;}Mix(std::fetestexcept(FE_ALL_EXCEPT));std::feclearexcept(FE_ALL_EXCEPT);assert(state==before);StencilSnapshot(state);return 0u;});++probes;
   }
 }
 assert(std::fesetround(original)==0);std::feclearexcept(FE_ALL_EXCEPT);assert(probes==2560 && cases==86791 && rejections==50832);
 std::printf("STENCIL_VALIDATE cases=%llu rejected=%llu checksum=%llu probe_cases=%u all8fields bounds20 enabled_two_sided4 rounding4_FPflags logical_state_unchanged CPU_ONLY_NOT_GPU\n",(unsigned long long)cases,(unsigned long long)rejections,(unsigned long long)hash,probes);
 return 0;
}

#include "matrixgame_state_ctor_probe.hpp"
int CtorLegacy(){assert(StencilLegacy()==0);const auto base=ConstructorAllocationProbe(-1);assert(!base.failed && !base.live && base.attempts>=103 && base.attempts<128);unsigned probes=0,failures=0;
 for(std::uint64_t n=0;n<=base.attempts+1;++n){Observe([&]{std::feclearexcept(FE_ALL_EXCEPT);const auto result=ConstructorAllocationProbe(std::int64_t(n));Mix(std::fetestexcept(FE_ALL_EXCEPT));std::feclearexcept(FE_ALL_EXCEPT);Mix(n);Mix(result.attempts);Mix(result.live);Mix(result.failed);Mix(result.checksum);assert(result.live==0);assert(result.failed==(n<base.attempts));
  if(result.failed){assert(result.attempts==n+1);++failures;}else{assert(result.attempts==base.attempts && result.checksum==base.checksum);}return 0u;});++probes;
 }
 assert(failures==base.attempts && probes==base.attempts+2 && cases==86791+probes && rejections==50832);
 std::printf("STATE_CTOR cases=%llu rejected=%llu checksum=%llu allocation_cases=%u injected_failures=%u ctor_allocations=%llu all103_defaults live_blocks_zero after_each_bad_alloc real_constructor_destructor CPU_ONLY_NOT_GPU\n",(unsigned long long)cases,(unsigned long long)rejections,(unsigned long long)hash,probes,failures,(unsigned long long)base.attempts);
 return 0;
}

#include <limits>
namespace br=srhd_awa::matrixgame::buffers;
std::uint64_t queries=0,range_states=0;
void VerifyRanges(br::Resource& buffer,const std::array<bool,128>& model){
 std::array<unsigned,129> missing{};for(unsigned i=0;i<128;++i)missing[i+1]=missing[i]+!model[i];const auto version=buffer.Version();assert(!buffer.Locked());
 for(std::size_t offset=0;offset<=128;++offset)for(std::size_t bytes=0;bytes<=128;++bytes){const bool expected=bytes<=128-offset && missing[offset+bytes]-missing[offset]==0;const auto value=buffer.Initialized(offset,bytes);assert(value==expected);Mix(value);++queries;}
 for(auto offset:{std::size_t(0),std::size_t(128),std::size_t(129),std::numeric_limits<std::size_t>::max()})for(auto bytes:{std::size_t(0),std::size_t(1),std::size_t(129),std::numeric_limits<std::size_t>::max()}){const bool expected=offset<=128 && bytes<=128-offset && missing[offset+bytes]-missing[offset]==0;const auto value=buffer.Initialized(offset,bytes);assert(value==expected);Mix(value);++queries;}
 assert(buffer.Version()==version && !buffer.Locked());++range_states;
}
int main(){assert(CtorLegacy()==0);br::Context context(4096);std::uint64_t allocation_cases=cases;
 for(unsigned type=0;type<2;++type){br::Resource* buffer=type?static_cast<br::Resource*>(context.CreateIndex(128,br::IndexWidth::U16,br::Pool::Default,true)):static_cast<br::Resource*>(context.CreateVertex(128,0x112,br::Pool::Default,true));assert(buffer);std::array<bool,128> model{};
  if(type)context.BindIndex(static_cast<br::IndexBuffer*>(buffer));else context.BindVertex(static_cast<br::VertexBuffer*>(buffer),0,32);
  Observe([&]{VerifyRanges(*buffer,model);return 0u;});
  auto write=[&](unsigned offset,unsigned bytes,br::MapMode mode){Observe([&]{const auto version=buffer->Version();const unsigned extent=!offset&&!bytes?128:bytes;auto* p=buffer->Map(offset,bytes,mode);assert(buffer->Locked());
   for(auto o:{std::size_t(0),std::size_t(128),std::size_t(129),std::numeric_limits<std::size_t>::max()})for(auto n:{std::size_t(0),std::size_t(1),std::size_t(129),std::numeric_limits<std::size_t>::max()}){assert(!buffer->Initialized(o,n));Mix(0);++queries;}
   std::memset(p,0x5a,extent);buffer->Unmap();if(mode==br::MapMode::Discard)model.fill(false);for(unsigned i=offset;i<offset+extent;++i)model[i]=true;assert(buffer->Version()==version+1);VerifyRanges(*buffer,model);return 0u;
  });};
  for(unsigned i=0;i<64;++i)write(i*2,1,br::MapMode::Normal);
  write(1,126,br::MapMode::Normal);write(127,1,br::MapMode::NoOverwrite);write(0,0,br::MapMode::Normal);
  unsigned seed=0x12345678u;for(unsigned i=0;i<64;++i){seed=seed*1664525u+1013904223u;const auto offset=seed%128;const auto bytes=1+(seed>>8)%(128-offset);write(offset,bytes,i%7==0?br::MapMode::Discard:i%2?br::MapMode::NoOverwrite:br::MapMode::Normal);}
  write(64,1,br::MapMode::Discard);context.ClearBindings();buffer->Release();assert(context.AllocatedBytes()==0);
 }
 assert(range_states==266 && cases-allocation_cases==266);
 std::printf("RANGE_SEARCH cases=%llu rejected=%llu checksum=%llu range_states=%llu queries=%llu bitmap_prefix_oracle all_offset_lengths vertex_index locked_zero_overflow fragmented_merged_discard version_unchanged_by_reads CPU_ONLY_NOT_GPU\n",(unsigned long long)cases,(unsigned long long)rejections,(unsigned long long)hash,(unsigned long long)range_states,(unsigned long long)queries);
}
