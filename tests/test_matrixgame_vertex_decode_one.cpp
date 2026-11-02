// Exact extracted indexed gather + real buffer Context. Record is a CPU data
// sink, not a GPU renderer or successful original Init/Draw execution.
#include "fixed_vertex.hpp"
#include "platform_3g.hpp"
#include "staging_draw.hpp"
#include "native_draw_batch.hpp"
#include <deko3d.h>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <limits>
std::size_t heap_calls{};bool deny_heap{};
void* operator new(std::size_t n){if(deny_heap)throw std::bad_alloc();auto* p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();++heap_calls;return p;}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace mg=srhd_awa::matrixgame;namespace mat=mg::materials;namespace materials=mg::materials;namespace buffers=mg::buffers;namespace staging_draw=mg::staging_draw;
using namespace mg::platform3g;
#include "native-primitive.inc"
struct IndexedHarness {
    struct State {mat::FixedVertexState vertex;};
    struct Store {State value;const State& Read()const{return value;}};
    struct Impl {
        static constexpr unsigned frame_budget=NativeDrawBatch::ByteLimit;
        Store state;std::vector<mat::Vertex> vertices;std::vector<std::byte> indices;DkIdxFormat format{};unsigned count{};int base{};DkPrimitive primitive{};
        void Record(std::span<const mat::Vertex> v,std::span<const std::byte> i,DkIdxFormat f,unsigned c,int b,DkPrimitive p){vertices.assign(v.begin(),v.end());indices.assign(i.begin(),i.end());format=f;count=c;base=b;primitive=p;}
    } data;
    Impl* impl_{&data};
    DeviceResult Baseline(std::uint32_t,int,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t);
    DeviceResult Candidate(std::uint32_t,int,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t);
};
#include "baseline-gather.inc"
#include "candidate-gather.inc"
std::vector<std::byte> Payload(std::uint32_t fvf,unsigned stride,unsigned count){
    std::vector<std::byte> out(std::size_t(stride)*count,std::byte{0xa5});
    for(unsigned n=0;n<count;++n){auto* p=out.data()+n*stride;auto append=[&](const void* from,unsigned size){std::memcpy(p,from,size);p+=size;};
        const float pos[]{float(n+1)/8,-float(n+2)/16,.375f,.5f},normal[]{.25f,.5f,.75f},uv[]{.125f,.25f,.5f,.75f};
        append(pos,(fvf&0xe)==4?16:12);if(fvf&0x10)append(normal,12);
        for(unsigned flag:{0x40u,0x80u})if(fvf&flag){const unsigned color=0xb14b87d3u+n;append(&color,4);}
        for(unsigned j=0;j<((fvf>>8)&15);++j){unsigned encoded=(fvf>>(16+2*j))&3;append(uv,4*(encoded==0?2:encoded==1?3:encoded==2?4:1));}
    }return out;
}
bool Equal(const mat::Vertex& a,const mat::Vertex& b){return std::memcmp(&a,&b,sizeof(a))==0;}
template<class F>bool Invalid(F f){try{f();}catch(const std::invalid_argument&){return true;}return false;}
int main(){
    unsigned layouts{},vertices{},invalid{};std::size_t avoided{};unsigned indexed{};
    for(unsigned position:{2u,4u})for(unsigned normal:{0u,0x10u})for(unsigned colors:{0u,0x40u,0x80u,0xc0u})for(unsigned count=0;count<=8;++count)for(unsigned pattern=0;pattern<4;++pattern){
        if(position==4 && normal)continue;std::uint32_t fvf=position|normal|colors|(count<<8);
        for(unsigned j=0;j<count;++j)fvf|=std::uint32_t((j+pattern)&3)<<(16+2*j);
        mat::FixedVertexState state;state.Layout(fvf);
        for(unsigned padding:{0u,12u}){const auto stride=state.Stride()+padding;auto bytes=Payload(fvf,stride,4);auto expected=state.Decode(bytes,stride);
            for(unsigned n=0;n<4;++n){auto input=std::span{bytes}.subspan(n*stride,stride);const auto before=heap_calls;auto single=state.DecodeOne(input,stride);assert(heap_calls==before && Equal(single,expected[n]));
                deny_heap=true;bool failed=false;try{single=state.DecodeOne(input,stride);}catch(...){failed=true;}deny_heap=false;assert(!failed && Equal(single,expected[n]));++vertices;
            }++layouts;
            auto first=std::span{bytes}.first(stride);assert(Invalid([&]{state.DecodeOne(first,stride-1);}));assert(Invalid([&]{state.DecodeOne(first.first(stride-1),stride);}));invalid+=2;
            auto broken=std::vector<std::byte>(first.begin(),first.end());const auto nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(broken.data(),&nan,4);
            assert(Invalid([&]{state.Decode(broken,stride);}));assert(Invalid([&]{state.DecodeOne(broken,stride);}));++invalid;
        }
    }
    for(unsigned fvf:{0x112u,0x212u,0x352u,0x3d2u,0x10112u,0x144u})for(auto width:{buffers::IndexWidth::U16,buffers::IndexWidth::U32}){
        IndexedHarness old,next;old.data.state.value.vertex.Layout(fvf);next.data.state.value.vertex.Layout(fvf);const auto stride=old.data.state.value.vertex.Stride()+4;
        auto bytes=Payload(fvf,stride,6);buffers::Context context;buffers::Scope scope(context);
        auto* vb=context.CreateVertex(bytes.size(),fvf);assert(vb);std::memcpy(vb->Map(0,0),bytes.data(),bytes.size());vb->Unmap();context.BindVertex(vb,0,stride);vb->Release();
        auto* ib=context.CreateIndex(8*unsigned(width),width);assert(ib);const unsigned input[]{99,99,4,5,4,7,5,4};auto* dst=static_cast<std::byte*>(ib->Map(0,0));
        for(unsigned n=0;n<8;++n){if(width==buffers::IndexWidth::U16){const std::uint16_t value=input[n];std::memcpy(dst+n*2,&value,2);}else std::memcpy(dst+n*4,&input[n],4);}ib->Unmap();context.BindIndex(ib);ib->Release();
        for(unsigned repeat=0;repeat<512;++repeat){const auto start=heap_calls;assert(old.Baseline(4,-2,4,4,2,2)==0);const auto original_allocations=heap_calls-start;
            const auto before=heap_calls;assert(next.Candidate(4,-2,4,4,2,2)==0);const auto candidate_allocations=heap_calls-before;
            assert(original_allocations==candidate_allocations+3);avoided+=original_allocations-candidate_allocations;
            assert(old.data.vertices.size()==next.data.vertices.size() && old.data.indices==next.data.indices);
            assert(std::memcmp(old.data.vertices.data(),next.data.vertices.data(),old.data.vertices.size()*sizeof(mat::Vertex))==0);
            assert(old.data.base==next.data.base && old.data.format==next.data.format && old.data.primitive==next.data.primitive && old.data.count==next.data.count);++indexed;
        }
        // Bounds/negative effective vertex rejection remains in the real gather.
        bool a=false,b=false;try{old.Baseline(4,-99,4,4,2,2);}catch(const std::out_of_range&){a=true;}try{next.Candidate(4,-99,4,4,2,2);}catch(const std::out_of_range&){b=true;}assert(a && b);
        context.ClearBindings();assert(context.AllocatedBytes()==0);
    }
    std::printf("DECODE_ONE PASS layouts=%u vertices=%u invalid_cases=%u indexed_gathers=%u avoided_heap_allocations=%zu bit_exact_batch_oracle actual_indexed_body retained_buffers CPU_ONLY NO_GPU_NO_FPS_CLAIM\n",layouts,vertices,invalid,indexed,avoided);
}
