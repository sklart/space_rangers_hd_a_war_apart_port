// Validate real staging extents before an SDK draw, including strip topology.
#ifdef NDEBUG
#error This runtime contract requires active assertions.
#endif
#include "staging_draw.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace srhd_awa::matrixgame;
template<class F> void Refused(F call){bool rejected=false;try{call();}catch(const std::exception&){rejected=true;}assert(rejected);}
int main(){
    buffers::Context context;
    auto* vertex=context.CreateVertex(64,2);
    auto* index=context.CreateIndex(8,buffers::IndexWidth::U16);
    {auto* p=static_cast<float*>(vertex->Map(0,48));for(int i=0;i<12;++i)p[i]=float(i);vertex->Unmap();}
    {auto* p=static_cast<std::uint16_t*>(index->Map(0,8));for(int i=0;i<4;++i)p[i]=i;index->Unmap();}
    context.BindVertex(vertex,0,16);context.BindIndex(index);
    staging_draw::ValidateIndexed(context,4,0,0,3,0,1);
    Refused([&]{staging_draw::ValidateIndexed(context,5,0,0,4,0,2);}); // Fourth vertex is unpublished.
    {auto* p=static_cast<float*>(vertex->Map(48,16));for(int i=0;i<4;++i)p[i]=float(i+12);vertex->Unmap();}
    staging_draw::ValidateIndexed(context,5,0,0,4,0,2);
    staging_draw::ValidateIndexed(context,4,0,1,3,1,1);
    Refused([&]{staging_draw::ValidateIndexed(context,4,-1,0,3,0,1);});
    Refused([&]{staging_draw::ValidateIndexed(context,4,0,1,3,0,1);});
    Refused([&]{staging_draw::ValidateIndexed(context,5,0,0,4,0,3);});
    Refused([&]{staging_draw::ValidateIndexed(context,4,0,0,4,0,0xffffffffu);});
    Refused([&]{staging_draw::ValidateIndexed(context,99,0,0,4,0,1);});
    Refused([&]{staging_draw::Vertex({vertex,std::numeric_limits<std::size_t>::max(),16},1);});
    Refused([&]{staging_draw::Vertex({vertex,0,std::numeric_limits<std::size_t>::max()},0x7fffffff);});
    auto* wide=context.CreateIndex(12,buffers::IndexWidth::U32);
    {auto* p=static_cast<std::uint32_t*>(wide->Map(0,12));p[0]=0;p[1]=70000;p[2]=2;wide->Unmap();}
    context.BindIndex(wide);
    Refused([&]{staging_draw::ValidateIndexed(context,4,0,0,3,0,1);}); // No u16 truncation.
    {auto* p=static_cast<std::uint32_t*>(wide->Map(0,12));p[0]=2;p[1]=1;p[2]=0;wide->Unmap();}
    staging_draw::ValidateIndexed(context,4,0,0,3,0,1);
    wide->Map(0,12);Refused([&]{staging_draw::ValidateIndexed(context,4,0,0,3,0,1);});wide->Unmap();
    assert(staging_draw::ElementCount(2,2)==4 && staging_draw::ElementCount(3,2)==3 && staging_draw::ElementCount(6,2)==4);
    vertex->Release();index->Release();wide->Release();context.ClearBindings();assert(!context.AllocatedBytes());
    std::puts("staging initialized ranges, strips, base/minimum, overflow, u32 and cleanup PASS; GPU NOT RUN");
}
