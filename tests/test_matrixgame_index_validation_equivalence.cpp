#include "staging_draw.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
using namespace srhd_awa::matrixgame;
void Reference(const buffers::Context& context,std::uint32_t topology,int base,std::uint32_t minimum,std::uint32_t vertices,std::uint32_t start,std::uint32_t primitives){
 const auto binding=context.Vertex();const auto* index=context.Index();
 if(!index || index->Locked())throw std::invalid_argument("staging index missing/locked");
 const auto width=static_cast<std::uint32_t>(index->Width());const auto first=std::uint64_t(start)*width,count=staging_draw::ElementCount(topology,primitives);
 if(first>index->Size() || count>(index->Size()-first)/width || !index->Initialized(first,count*width))throw std::out_of_range("staging index range");
 const auto bytes=index->Storage();
 for(std::uint64_t i=0;i<count;++i){std::uint32_t value{};if(width==2){std::uint16_t v;std::memcpy(&v,bytes.data()+first+i*width,2);value=v;}else std::memcpy(&value,bytes.data()+first+i*width,4);
  if(value<minimum || std::uint64_t(value)-minimum>=vertices)throw std::out_of_range("staging index outside declared vertex range");
  staging_draw::Vertex(binding,std::int64_t(base)+value);
 }
}
template<class F>std::string Outcome(F fn){try{fn();return "PASS";}catch(const std::exception& e){return e.what();}}
int main(){
 std::uint32_t random=1234567;auto next=[&](){random=random*1664525u+1013904223u;return random;};unsigned pass=0,fail=0;
 for(unsigned trial=0;trial<1000;++trial){
  buffers::Context context;auto* vb=context.CreateVertex(64*16,2);const unsigned initialized=trial%3?64:48;
  std::memset(vb->Map(0,initialized*16),0,initialized*16);vb->Unmap();context.BindVertex(vb,0,16);
  const auto width=trial%2?buffers::IndexWidth::U16:buffers::IndexWidth::U32;auto* ib=context.CreateIndex(120*unsigned(width),width);
  auto* out=static_cast<std::byte*>(ib->Map(0,0));std::uint32_t earlier=0;
  for(unsigned i=0;i<120;++i){auto value=(i%5==0)?earlier:(next()>>(trial%13))%(trial%4?48:72);earlier=value;
   if(width==buffers::IndexWidth::U16){auto short_value=std::uint16_t(value);std::memcpy(out+i*2,&short_value,2);}else std::memcpy(out+i*4,&value,4);
  }
  ib->Unmap();context.BindIndex(ib);const int base=trial%7?0:-1;
  auto a=Outcome([&]{Reference(context,4,base,0,64,0,40);});auto b=Outcome([&]{staging_draw::ValidateIndexed(context,4,base,0,64,0,40);});
  assert(a==b);if(a=="PASS")++pass;else ++fail;
  context.ClearBindings();vb->Release();ib->Release();assert(!context.AllocatedBytes());
 }
 assert(pass && fail);std::printf("INDEX VALIDATION PASS 1000 reference comparisons pass=%u reject=%u; CPU ONLY\n",pass,fail);
}
