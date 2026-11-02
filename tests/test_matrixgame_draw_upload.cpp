// Production NativeDrawUpload and real SDK owners, explicit CPU memory endpoints.
// Does not submit commands, emulate a GPU or claim original Init/Draw execution.
#include "native_draw_upload.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <type_traits>
#include <vector>
namespace p=srhd_awa::matrixgame::platform3g;
namespace dk::detail {struct Device{};struct MemBlock {std::vector<std::byte> bytes;DkGpuAddr gpu;};}
dk::detail::Device device;
std::vector<DkMemBlock> live;
std::uint64_t next_gpu=0x100000;unsigned allocations{},fail_allocation{};
extern "C" DkMemBlock dkMemBlockCreate(const DkMemBlockMaker* maker){
    ++allocations;if(allocations==fail_allocation)return nullptr;
    assert(maker->size && maker->size%DK_MEMBLOCK_ALIGNMENT==0);
    auto* memory=new dk::detail::MemBlock{std::vector<std::byte>(maker->size,std::byte{0xcd}),next_gpu};
    next_gpu+=maker->size+DK_MEMBLOCK_ALIGNMENT;live.push_back(memory);return memory;
}
extern "C" void dkMemBlockDestroy(DkMemBlock memory){auto it=std::find(live.begin(),live.end(),memory);assert(it!=live.end());live.erase(it);delete memory;}
extern "C" void* dkMemBlockGetCpuAddr(DkMemBlock memory){return memory->bytes.data();}
extern "C" DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock memory){return memory->gpu;}
extern "C" std::uint32_t dkMemBlockGetSize(DkMemBlock memory){return std::uint32_t(memory->bytes.size());}
#include "allocation-size.inc"
dk::UniqueMemBlock Allocate(std::uint64_t bytes){
    dk::UniqueMemBlock memory=dk::MemBlockMaker{&device,AllocationSize(bytes)}.setFlags(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached).create();
    if(!memory)throw std::bad_alloc();return std::move(memory);
}
struct BaselinePacket {dk::UniqueMemBlock vertices,indices,sampler_descriptors,image_descriptors;};
BaselinePacket Baseline(std::span<const std::byte> vertices,std::span<const std::byte> indices){
#include "baseline-upload.inc"
    return packet;
}
std::span<const std::byte> Bytes(DkGpuAddr address,std::size_t size){
    for(auto memory:live)if(address>=memory->gpu && address-memory->gpu<=memory->bytes.size() && size<=memory->bytes.size()-(address-memory->gpu))
        return {memory->bytes.data()+std::size_t(address-memory->gpu),size};
    assert(false);return {};
}
void Same(DkGpuAddr a,DkGpuAddr b,std::size_t n){assert(std::equal(Bytes(a,n).begin(),Bytes(a,n).end(),Bytes(b,n).begin()));}
template<class F>void Reject(F fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}assert(rejected);}
p::NativeDrawUpload Make(std::span<const std::byte> vertices,std::span<const std::byte> indices){
    const auto layout=p::NativeDrawUpload::Plan(vertices.size(),indices.size());
    return {Allocate(layout.bytes),vertices,indices};
}
int main(){
    static_assert(std::is_nothrow_move_constructible_v<p::NativeDrawUpload>);
    static_assert(std::is_move_assignable_v<p::NativeDrawUpload>); // SDK assignment has no noexcept annotation.
    unsigned cases{},old_allocs{},new_allocs{};
    for(unsigned v:{1u,4u,16u,192u,576u,4095u,4096u,4097u,8192u,16384u,65537u})
    for(unsigned i:{0u,2u,4u,6u,4095u,4096u,4097u,32769u}){
        assert(live.empty());
        {
            std::vector<std::byte> vertices(v),indices(i);for(unsigned n=0;n<v;++n)vertices[n]=std::byte(n*71u+3u);for(unsigned n=0;n<i;++n)indices[n]=std::byte(n*13u+5u);
            const auto plan=p::NativeDrawUpload::Plan(v,i);const auto before=allocations;auto old=Baseline(vertices,indices);old_allocs+=allocations-before;assert(allocations-before==(i?4u:3u));
            auto start=allocations;auto upload=Make(vertices,indices);new_allocs+=allocations-start;assert(allocations-start==1);
            assert(plan.bytes==AllocationSize(v)+(i?AllocationSize(i):0)+AllocationSize(8*sizeof(DkSamplerDescriptor))+AllocationSize(8*sizeof(DkImageDescriptor)));
            assert(upload.Vertices()%DK_MEMBLOCK_ALIGNMENT==0 && upload.Indices()%4==0);
            assert(upload.Samplers()%DK_SAMPLER_DESCRIPTOR_ALIGNMENT==0 && upload.Images()%DK_IMAGE_DESCRIPTOR_ALIGNMENT==0);
            assert(upload.Indices()==upload.Vertices()+plan.indices && upload.Samplers()==upload.Vertices()+plan.samplers && upload.Images()==upload.Vertices()+plan.images);
            assert(plan.indices>=v && plan.samplers>=plan.indices+i && plan.images>=plan.samplers+8*sizeof(DkSamplerDescriptor) && plan.bytes>=plan.images+8*sizeof(DkImageDescriptor));
            for(unsigned slot=0;slot<8;++slot){
                DkSamplerDescriptor sampler;std::memset(&sampler,int(slot*17+3),sizeof(sampler));upload.WriteSampler(slot,sampler);
                std::memcpy(static_cast<std::byte*>(old.sampler_descriptors.getCpuAddr())+slot*sizeof(sampler),&sampler,sizeof(sampler));
                if(slot%2){DkImageDescriptor image;std::memset(&image,int(slot*19+7),sizeof(image));upload.WriteImage(slot,image);std::memcpy(static_cast<std::byte*>(old.image_descriptors.getCpuAddr())+slot*sizeof(image),&image,sizeof(image));}
            }
            Same(upload.Vertices(),old.vertices.getGpuAddr(),v);if(i)Same(upload.Indices(),old.indices.getGpuAddr(),i);
            Same(upload.Samplers(),old.sampler_descriptors.getGpuAddr(),8*sizeof(DkSamplerDescriptor));Same(upload.Images(),old.image_descriptors.getGpuAddr(),8*sizeof(DkImageDescriptor));
            for(auto gap:std::array<std::pair<std::uint32_t,std::uint32_t>,4>{{{v,plan.indices},{plan.indices+i,plan.samplers},{plan.samplers+8*sizeof(DkSamplerDescriptor),plan.images},{plan.images+8*sizeof(DkImageDescriptor),plan.bytes}}})
                for(auto byte:Bytes(upload.Vertices()+gap.first,gap.second-gap.first))assert(byte==std::byte{0xcd});
            const auto address=upload.Vertices();auto moved=std::move(upload);Reject([&]{upload.Vertices();});assert(moved.Vertices()==address);Same(moved.Vertices(),old.vertices.getGpuAddr(),v);
            DkImageDescriptor image{};DkSamplerDescriptor sampler{};Reject([&]{moved.WriteImage(8,image);});Reject([&]{moved.WriteSampler(UINT32_MAX,sampler);});++cases;
        }
        assert(live.empty());
    }
    std::vector<std::byte> v(576),i(6);
    for(unsigned fault=1;fault<=4;++fault){fail_allocation=allocations+fault;Reject([&]{Baseline(v,i);});fail_allocation=0;assert(live.empty());}
    fail_allocation=allocations+1;Reject([&]{Make(v,i);});fail_allocation=0;assert(live.empty());
    Reject([&]{p::NativeDrawUpload bad{Allocate(DK_MEMBLOCK_ALIGNMENT),v,i};});assert(live.empty());
    Reject([&]{p::NativeDrawUpload bad{{},v,i};});assert(live.empty());
    for(auto sizes:std::array<std::pair<std::uint64_t,std::uint64_t>,5>{{{0,0},{UINT64_MAX,0},{1,UINT64_MAX},{UINT32_MAX,0},{UINT32_MAX-DK_MEMBLOCK_ALIGNMENT,8192}}}){
        const auto count=allocations;Reject([&]{p::NativeDrawUpload::Plan(sizes.first,sizes.second);});assert(count==allocations);
    }
    {
        std::vector<p::NativeDrawUpload> retained;std::vector<DkGpuAddr> addresses;
        for(unsigned n=0;n<257;++n){retained.push_back(Make(v,i));addresses.push_back(retained.back().Vertices());}
        assert(live.size()==retained.size());for(unsigned n=0;n<retained.size();++n)assert(retained[n].Vertices()==addresses[n]);
        // Moving packets during vector growth retains backing. Retirement alone releases it.
        retained.clear();assert(live.empty());
    }
    {auto left=Make(v,i);auto right=Make(v,i);auto address=left.Vertices();right=std::move(left);assert(live.size()==1 && right.Vertices()==address);}
    assert(live.empty() && cases==88 && old_allocs==341 && new_allocs==88);
    std::printf("DRAW_UPLOAD PASS layouts=%u baseline_blocks=%u candidate_blocks=%u avoided_blocks=%u exact_baseline_payload descriptor_alignment padding_bounds fault_unwind move_retention CPU_SDK_ENDPOINTS NO_GPU_NO_FPS\n",cases,old_allocs,new_allocs,old_allocs-new_allocs);
}
