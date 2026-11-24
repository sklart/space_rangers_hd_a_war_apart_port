#include "experimental_draw_arena.hpp"
#include "native_draw_upload.hpp"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <vector>
using namespace srhd_awa::matrixgame::platform3g;
template<class F>bool Refused(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
 using experimental::DrawArena;
 assert(Refused([]{DrawArena bad(1024,3);}));
 DrawArena arena(4*1024*1024,DK_MEMBLOCK_ALIGNMENT);
 struct Marker{std::uint64_t offset,length;unsigned char value;};
 std::vector<std::byte> simulated_memory(4*1024*1024);std::vector<Marker> live;
 auto verify=[&]{for(const auto& x:live){assert(simulated_memory[x.offset]==std::byte{x.value});assert(simulated_memory[x.offset+x.length-1]==std::byte{x.value});}};
 std::uint64_t end=0,draws=0,submissions=0;std::vector<std::uint64_t> offsets;
 for(unsigned i=0;i<4096;++i){
  const auto layout=NativeDrawUpload::Plan(128+(i%9)*112,(i%2)*192,true);
  auto offset=arena.Append(layout.bytes);
  if(!offset){
   verify();arena.Submit();assert(!arena.Complete(false));assert(arena.Used()==end);verify();
   assert(Refused([&]{arena.Append(layout.bytes);}));assert(arena.Complete(true));
   ++submissions;end=0;live.clear();offset=arena.Append(layout.bytes);
  }
  assert(offset && *offset%DK_MEMBLOCK_ALIGNMENT==0 && *offset>=end);
  assert(*offset+layout.vertices>=*offset && *offset+layout.indices<*offset+layout.bytes);
  assert((*offset+layout.images)%DK_IMAGE_DESCRIPTOR_ALIGNMENT==0);
  assert((*offset+layout.samplers)%DK_SAMPLER_DESCRIPTOR_ALIGNMENT==0);
  assert((*offset+layout.uniforms)%DK_UNIFORM_BUF_ALIGNMENT==0);
  end=*offset+layout.bytes;assert(end<=4*1024*1024);offsets.push_back(*offset);++draws;
  const auto marker=static_cast<unsigned char>(i%251+1);
  simulated_memory[*offset]=std::byte{marker};simulated_memory[end-1]=std::byte{marker};
  live.push_back({*offset,layout.bytes,marker});
 }
 if(arena.Used()){verify();arena.Submit();assert(arena.Complete(true));++submissions;}
 assert(draws==4096 && submissions<64 && offsets.size()==4096);
 assert(Refused([&]{arena.Append(0);}));
 assert(!arena.Append(std::numeric_limits<std::uint64_t>::max()));
 std::printf("CPU arena planning PASS draws=%llu arena_epochs=%llu; GPU NOT RUN\n",(unsigned long long)draws,(unsigned long long)submissions);
}
