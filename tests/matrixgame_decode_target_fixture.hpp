#pragma once
#include "native_device_state.hpp"
#include "native_profile.hpp"
#include "staging_draw.hpp"
#include "native_draw_batch.hpp"
#include <deko3d.h>
#include <cassert>
#include <bit>
#include <cstring>
#include <cstdio>
namespace mg=srhd_awa::matrixgame;namespace mat=mg::materials;namespace materials=mg::materials;namespace buffers=mg::buffers;namespace staging_draw=mg::staging_draw;
using namespace mg::platform3g;
#include "native-primitive.inc"
struct IndexedHarness {
 struct Impl {
  static constexpr unsigned frame_budget=NativeDrawBatch::ByteLimit;
  NativeStateStore state;struct {ProfileTotals vertex_decode;} diagnostics;
  std::vector<mat::Vertex> vertices;std::vector<std::byte> indices;
  unsigned count{},records{};int base{};DkIdxFormat format{};DkPrimitive primitive{};
  std::uint64_t checksum=1469598103934665603ull;
  void Record(std::span<const mat::Vertex> v,std::span<const std::byte> i,DkIdxFormat f,unsigned c,int b,DkPrimitive p){
   ++records;count=c;base=b;format=f;primitive=p;
#ifdef BENCHMARK_DECODE
   // CPU-only consumption, no GPU frame or execution claim.
   checksum=(checksum^std::bit_cast<unsigned>(v.front().position[0]))*1099511628211ull;
   checksum=(checksum^std::bit_cast<unsigned>(v.back().uv[0][1]))*1099511628211ull;
   checksum=(checksum^c)*1099511628211ull;
#else
   vertices.assign(v.begin(),v.end());indices.assign(i.begin(),i.end());
#endif
  }
 } data;
 Impl* impl_{&data};
 DeviceResult Gather(std::uint32_t,int,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t);
};
#include "gather.inc"
inline void ProbeDecode(const mat::FixedVertexState& s,mat::Vertex& out,std::span<const std::byte> bytes,unsigned stride){
#ifdef DIRECT_DECODE
 s.DecodeTo(out,bytes,stride);
#else
 out=s.DecodeOne(bytes,stride);
#endif
}
inline std::vector<std::byte> Payload(unsigned fvf,unsigned stride,unsigned count){
 std::vector<std::byte> out(std::size_t(stride)*count,std::byte{0xa5});
 for(unsigned n=0;n<count;++n){auto* p=out.data()+n*stride;auto append=[&](const void* x,unsigned size){std::memcpy(p,x,size);p+=size;};
  const float pos[]{float(n+1)/8,-float(n+2)/16,.375f,.5f},normal[]{.25f,.5f,.75f},uv[]{.125f,.25f,.5f,.75f};
  append(pos,(fvf&0xe)==4?16:12);if(fvf&0x10)append(normal,12);
  for(unsigned flag:{0x40u,0x80u})if(fvf&flag){const unsigned c=0xb14b87d3u+n;append(&c,4);}
  for(unsigned j=0;j<((fvf>>8)&15);++j){unsigned d=(fvf>>(16+2*j))&3;append(uv,4*(d==0?2:d==1?3:d==2?4:1));}
 }return out;
}
inline void Bind(buffers::Context& context,const std::vector<std::byte>& bytes,unsigned fvf,unsigned stride,std::span<const unsigned> input,buffers::IndexWidth width){
 auto* vb=context.CreateVertex(bytes.size(),fvf);assert(vb);std::memcpy(vb->Map(0,0),bytes.data(),bytes.size());vb->Unmap();context.BindVertex(vb,0,stride);vb->Release();
 auto* ib=context.CreateIndex(input.size()*unsigned(width),width);assert(ib);auto* dst=static_cast<std::byte*>(ib->Map(0,0));
 for(unsigned i=0;i<input.size();++i)if(width==buffers::IndexWidth::U16){const std::uint16_t value=input[i];std::memcpy(dst+i*2,&value,2);}else std::memcpy(dst+i*4,&input[i],4);
 ib->Unmap();context.BindIndex(ib);ib->Release();
}
