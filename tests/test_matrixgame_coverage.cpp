#ifdef NDEBUG
#error Keep decode assertions
#endif
#include "matrixgame_decode_target_fixture.hpp"
#include <cstdlib>
#include <new>
#include <limits>
std::size_t heap_calls{};bool deny_heap{};
void* operator new(std::size_t n){if(deny_heap)throw std::bad_alloc();auto* p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();++heap_calls;return p;}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
std::uint64_t hash=1469598103934665603ull,cases{},rejected{};unsigned layouts{},gathers{},no_publication{};
void Mix(unsigned x){hash=(hash^x)*1099511628211ull;}
void Bytes(const void* data,std::size_t size){auto p=static_cast<const unsigned char*>(data);for(std::size_t i=0;i<size;++i)Mix(p[i]);}
template<class F>void Observe(F f){Mix(unsigned(++cases));try{f();Mix(1);}catch(const std::exception& e){++rejected;Mix(0);Mix(dynamic_cast<const std::out_of_range*>(&e)?1:dynamic_cast<const std::invalid_argument*>(&e)?2:3);for(auto p=e.what();*p;++p)Mix(static_cast<unsigned char>(*p));}}
int DecodeRegression(){
 for(unsigned position:{2u,4u})for(unsigned normal:{0u,0x10u})for(unsigned colors:{0u,0x40u,0x80u,0xc0u})for(unsigned count=0;count<=8;++count)for(unsigned pattern=0;pattern<4;++pattern){
  if(position==4 && normal)continue;unsigned fvf=position|normal|colors|(count<<8);for(unsigned j=0;j<count;++j)fvf|=((j+pattern)&3)<<(16+2*j);
  mat::FixedVertexState state;state.Layout(fvf);
  for(unsigned padding:{0u,12u}){++layouts;const auto stride=state.Stride()+padding;auto input=Payload(fvf,stride,4);const auto expected=state.Decode(input,stride);
   for(unsigned n=0;n<4;++n)Observe([&]{mat::Vertex v;std::memset(&v,0xa5,sizeof(v));const auto calls=heap_calls;deny_heap=true;try{ProbeDecode(state,v,std::span{input}.subspan(n*stride,stride),stride);}catch(...){deny_heap=false;throw;}deny_heap=false;assert(heap_calls==calls);assert(!std::memcmp(&v,&expected[n],sizeof(v)));Bytes(&v,sizeof(v));});
   for(unsigned bad:{0u,stride-1,0xffffffffu})Observe([&]{mat::Vertex v;ProbeDecode(state,v,std::span{input}.first(stride),bad);assert(false);});
   Observe([&]{mat::Vertex v;ProbeDecode(state,v,std::span{input}.first(stride-1),stride);assert(false);});
   // Nonfinite input must keep the same exception category/message.
   for(unsigned word:{0u,2u})Observe([&]{auto broken=input;const auto nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(broken.data()+word*4,&nan,4);mat::Vertex v;ProbeDecode(state,v,std::span{broken}.first(stride),stride);assert(false);});
   Observe([&]{auto broken=input;const auto nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(broken.data(),&nan,4);mat::Vertex v;std::memset(&v,0xa5,sizeof(v));bool failed=false;try{ProbeDecode(state,v,std::span{broken}.first(stride),stride);}catch(const std::invalid_argument&){failed=true;}assert(failed);ProbeDecode(state,v,std::span{input}.first(stride),stride);assert(!std::memcmp(&v,&expected[0],sizeof(v)));Bytes(&v,sizeof(v));});
  }
 }
 for(unsigned fvf:{0x112u,0x212u,0x352u,0x3d2u,0x10112u,0x144u})for(auto width:{buffers::IndexWidth::U16,buffers::IndexWidth::U32})for(int base:{-2,0,3}){
  IndexedHarness h;h.data.state.Edit().vertex.Layout(fvf);const auto stride=h.data.state.Read().vertex.Stride()+4;auto bytes=Payload(fvf,stride,12);const auto decoded=h.data.state.Read().vertex.Decode(bytes,stride);
  buffers::Context context;buffers::Scope scope(context);const unsigned indices[]{99,99,4,5,4,7,5,4};Bind(context,bytes,fvf,stride,indices,width);
  for(unsigned repeat=0;repeat<4;++repeat)Observe([&]{assert(h.Gather(4,base,4,4,2,2)==0);assert(h.data.vertices.size()==4 && h.data.count==6 && h.data.base==-4 && h.data.primitive==DkPrimitive_Triangles);
   for(unsigned local:{0u,1u,3u})assert(!std::memcmp(&h.data.vertices[local],&decoded[base+4+local],sizeof(mat::Vertex)));
   mat::Vertex hole;assert(!std::memcmp(&h.data.vertices[2],&hole,sizeof(hole)));
   assert(h.data.indices.size()==6*unsigned(width));for(unsigned i=0;i<6;++i){unsigned v{};if(width==buffers::IndexWidth::U16){std::uint16_t value;std::memcpy(&value,h.data.indices.data()+i*2,2);v=value;}else std::memcpy(&v,h.data.indices.data()+i*4,4);assert(v==indices[i+2]);}
   Bytes(h.data.vertices.data(),h.data.vertices.size()*sizeof(mat::Vertex));Bytes(h.data.indices.data(),h.data.indices.size());++gathers;
  });
  const auto published=h.data.records;auto before=h.data.vertices;
  for(unsigned fault=0;fault<5;++fault){Observe([&]{
    if(fault==0)h.Gather(4,-99,4,4,2,2);
    else if(fault==1)h.Gather(4,base,4,3,2,2);
    else if(fault==2)h.Gather(4,base,4,4,99,2);
    else if(fault==3){auto* vb=const_cast<buffers::VertexBuffer*>(context.Vertex().buffer);auto* dst=vb->Map(0,0);(void)dst;try{h.Gather(4,base,4,4,2,2);}catch(...){vb->Unmap();throw;}vb->Unmap();}
    else {auto* ib=const_cast<buffers::IndexBuffer*>(context.Index());auto* dst=ib->Map(0,0);(void)dst;try{h.Gather(4,base,4,4,2,2);}catch(...){ib->Unmap();throw;}ib->Unmap();}
    assert(false);
   });assert(h.data.records==published && h.data.vertices.size()==before.size() && !std::memcmp(before.data(),h.data.vertices.data(),before.size()*sizeof(mat::Vertex)));++no_publication;
  }
  // Initialized bytes with a late NaN pass staging bounds, fail decode, publish nothing.
  auto* vb=const_cast<buffers::VertexBuffer*>(context.Vertex().buffer);auto* data=static_cast<std::byte*>(vb->Map(0,0));const auto nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(data+std::size_t(base+7)*stride,&nan,4);vb->Unmap();
  Observe([&]{h.Gather(4,base,4,4,2,2);assert(false);});assert(h.data.records==published);++no_publication;
  context.ClearBindings();assert(context.AllocatedBytes()==0);
 }
 assert(layouts==864 && gathers==144 && no_publication==216);
 std::printf("DECODE_TARGET cases=%llu rejected=%llu layouts=%u gathers=%u no_publication=%u checksum=%llu full192_batch_oracle defaults_holes_indices_base_no_heap_decode_failure_retains_CPU_sink CPU_ONLY_NOT_GPU\n",(unsigned long long)cases,(unsigned long long)rejected,layouts,gathers,no_publication,(unsigned long long)hash);
 return 0;
}

extern std::uint64_t matrixgame_initialized_calls;
int main(){assert(DecodeRegression()==0);unsigned scenarios{},full_scopes{},partial_scopes{};
 for(auto width:{buffers::IndexWidth::U16,buffers::IndexWidth::U32}){
  IndexedHarness h;h.data.state.Edit().vertex.Layout(0x112);const unsigned stride=h.data.state.Read().vertex.Stride();auto bytes=Payload(0x112,stride,8);
  buffers::Context context;buffers::Scope scope(context);
  auto* vb=context.CreateVertex(bytes.size(),0x112,buffers::Pool::Default,true);assert(vb);
  std::memcpy(vb->Map(0,0),bytes.data(),bytes.size());vb->Unmap();context.BindVertex(vb,0,stride);vb->Release();
  auto* ib=context.CreateIndex(6*unsigned(width),width);assert(ib);auto write_indices=[&](unsigned first){const unsigned values[]{first,5,4,7,5,4};auto* out=static_cast<std::byte*>(ib->Map(0,0));for(unsigned i=0;i<6;++i)if(width==buffers::IndexWidth::U16){std::uint16_t value=std::uint16_t(values[i]);std::memcpy(out+i*2,&value,2);}else std::memcpy(out+i*4,&values[i],4);ib->Unmap();};write_indices(4);context.BindIndex(ib);
  matrixgame_initialized_calls=0;Observe([&]{assert(h.Gather(4,-2,4,4,0,2)==0);Bytes(h.data.vertices.data(),4*sizeof(mat::Vertex));});
#ifdef COVERAGE_VIEW
  assert(matrixgame_initialized_calls==2);
#else
  assert(matrixgame_initialized_calls==7);
#endif
  ++full_scopes;
  auto* owned=const_cast<buffers::VertexBuffer*>(context.Vertex().buffer);
  // Discard after a full draw invalidates coverage; the next draw must fail.
  std::memcpy(owned->Map(0,stride,buffers::MapMode::Discard),bytes.data(),stride);owned->Unmap();const auto records=h.data.records;
  Observe([&]{h.Gather(4,-2,4,4,0,2);assert(false);});assert(h.data.records==records);++scenarios;
  // The declared range contains row 4, but only rows 2,3,5 are referenced.
  for(unsigned row:{2u,3u,5u}){std::memcpy(owned->Map(row*stride,stride),bytes.data()+row*stride,stride);owned->Unmap();}
  matrixgame_initialized_calls=0;Observe([&]{assert(h.Gather(4,-2,4,4,0,2)==0);Bytes(h.data.vertices.data(),4*sizeof(mat::Vertex));});
#ifdef COVERAGE_VIEW
  assert(matrixgame_initialized_calls==8);
#else
  assert(matrixgame_initialized_calls==7);
#endif
  ++partial_scopes;
  write_indices(6);const auto before=h.data.records;Observe([&]{h.Gather(4,-2,4,4,0,2);assert(false);});assert(h.data.records==before);++scenarios;
  ib->Release();context.ClearBindings();assert(context.AllocatedBytes()==0);
 }
 // Same per-index errors across empty/partial/full buffers, including the
 // exact end-of-buffer point, large strides and negative effective indices.
 for(auto width:{buffers::IndexWidth::U16,buffers::IndexWidth::U32})for(unsigned initialized=0;initialized<3;++initialized){
  buffers::Context context;buffers::Scope scope(context);auto* vb=context.CreateVertex(128,0x112);assert(vb);
  if(initialized){std::memset(vb->Map(0,initialized==2?128:64),0,initialized==2?128:64);vb->Unmap();}
  auto* ib=context.CreateIndex(3*unsigned(width),width);assert(ib);context.BindIndex(ib);
  for(std::size_t stride:{std::size_t(1),std::size_t(32),std::size_t(33),std::size_t(128),std::size_t(129),std::numeric_limits<std::size_t>::max()})for(std::size_t offset:{std::size_t(0),std::size_t(1),std::size_t(127)})for(unsigned value:{0u,1u,3u,4u,5u,65535u})for(int base:{-1,0,1}){
   context.BindVertex(vb,offset,stride);auto* out=static_cast<std::byte*>(ib->Map(0,0));for(unsigned i=0;i<3;++i)if(width==buffers::IndexWidth::U16){std::uint16_t v=std::uint16_t(value);std::memcpy(out+i*2,&v,2);}else std::memcpy(out+i*4,&value,4);ib->Unmap();
   Observe([&]{staging_draw::ValidateIndexed(context,4,base,0,65536,0,1);});++scenarios;
  }
  // Declared-index rejection precedes missing VB; valid index reaches VB guard.
  std::memset(ib->Map(0,0),0,3*unsigned(width));ib->Unmap();context.BindVertex(nullptr,0,0);for(unsigned minimum:{0u,1u})Observe([&]{staging_draw::ValidateIndexed(context,4,0,minimum,1,0,1);});
  vb->Release();ib->Release();context.ClearBindings();assert(context.AllocatedBytes()==0);
 }
 assert(full_scopes==2 && partial_scopes==2 && scenarios==1948);
 std::printf("COVERAGE_VALIDATE cases=%llu rejected=%llu checksum=%llu scenarios=%u full_scopes=%u partial_scopes=%u full_call_counts_7_to_2 partial_7_to_8 actual_initialized_body read_only_scope_no_cache_across_discard CPU_ONLY\n",(unsigned long long)cases,(unsigned long long)rejected,(unsigned long long)hash,scenarios,full_scopes,partial_scopes);
}
