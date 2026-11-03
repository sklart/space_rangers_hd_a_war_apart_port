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
int main(){
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
 // Each draw must reacquire a new state and a newly bound VB.
 for(auto width:{buffers::IndexWidth::U16,buffers::IndexWidth::U32}){
  IndexedHarness h;h.data.state.Edit().vertex.Layout(0x112);const unsigned stride=32;auto bytes=Payload(0x112,stride,4);buffers::Context context;buffers::Scope scope(context);const unsigned input[]{0,1,2};Bind(context,bytes,0x112,stride,input,width);
  Observe([&]{assert(h.Gather(4,0,0,4,0,1)==0);Bytes(h.data.vertices.data(),h.data.vertices.size()*sizeof(mat::Vertex));});
  auto changed=Payload(0x12,stride,4);const float x=7.25f;std::memcpy(changed.data(),&x,4);auto* next=context.CreateVertex(changed.size(),0x12);assert(next);std::memcpy(next->Map(0,0),changed.data(),changed.size());next->Unmap();context.BindVertex(next,0,stride);next->Release();h.data.state.Edit().vertex.Layout(0x12);
  Observe([&]{assert(h.Gather(4,0,0,4,0,1)==0);assert(h.data.vertices[0].position[0]==x);assert(h.data.vertices[0].uv[0][0]==0 && h.data.vertices[0].uv[0][3]==1);Bytes(h.data.vertices.data(),h.data.vertices.size()*sizeof(mat::Vertex));});
  const auto published=h.data.records;h.data.state.Close();
  Observe([&]{deny_heap=true;try{h.Gather(4,0,0,4,0,1);}catch(const std::bad_alloc&){deny_heap=false;throw;}catch(...){deny_heap=false;assert(false);}deny_heap=false;assert(false);});assert(h.data.records==published);
  Observe([&]{h.Gather(4,0,0,4,0,1);assert(false);});assert(h.data.records==published);
  context.ClearBindings();assert(context.AllocatedBytes()==0);
 }
 assert(layouts==864 && gathers==144 && no_publication==216);
 std::printf("INPUT_SCOPE cases=%llu rejected=%llu layouts=%u gathers=%u no_publication=%u checksum=%llu full192_batch_oracle defaults_holes_indices_base_no_heap_decode_failure_retains_CPU_sink CPU_ONLY_NOT_GPU\n",(unsigned long long)cases,(unsigned long long)rejected,layouts,gathers,no_publication,(unsigned long long)hash);
}
