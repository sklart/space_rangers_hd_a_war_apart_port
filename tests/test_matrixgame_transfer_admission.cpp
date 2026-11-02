// Executes production DekoTextureStore/native_texture with SDK C protocol
// endpoints. Opaque layouts/copies are fixtures, never GPU/image proof.
#include "texture_store.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
namespace nt=srhd_awa::matrixgame::native_texture;
namespace dk::detail {
struct Device{};struct Queue{bool lost{},closed{};};
struct MemBlock{std::vector<std::byte> bytes;};struct CmdBuf{};
}
dk::detail::Device device;dk::detail::Queue queue;
unsigned allocations{},live_memory{},live_commands{},submits{},waits{},copies{},layouts{},finishes{},known_lost_dispatch{};
unsigned known_lost_flush{},known_lost_wait{};
unsigned lose_at_alloc{},fail_at_alloc{};bool lose_at_finish{},lose_at_wait{},lose_at_submit{},lose_at_flush{};
std::vector<DkMemBlock> memory_registry;
DkMemBlock latest_staging{},pending_staging{};
extern "C" DkMemBlock dkMemBlockCreate(const DkMemBlockMaker* maker){
    ++allocations;if(lose_at_alloc==allocations)queue.lost=true;if(fail_at_alloc==allocations)return nullptr;
    auto m=new dk::detail::MemBlock{std::vector<std::byte>(maker->size)};memory_registry.push_back(m);++live_memory;return m;
}
extern "C" void dkMemBlockDestroy(DkMemBlock m){assert(live_memory && (pending_staging!=m || queue.closed));if(pending_staging==m)pending_staging=nullptr;auto it=std::find(memory_registry.begin(),memory_registry.end(),m);assert(it!=memory_registry.end());memory_registry.erase(it);--live_memory;delete m;}
extern "C" void* dkMemBlockGetCpuAddr(DkMemBlock m){return m->bytes.data();}
extern "C" DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock m){latest_staging=m;return reinterpret_cast<std::uintptr_t>(m->bytes.data());}
extern "C" DkCmdBuf dkCmdBufCreate(const DkCmdBufMaker*){++live_commands;return new dk::detail::CmdBuf;}
extern "C" void dkCmdBufDestroy(DkCmdBuf c){assert(live_commands);--live_commands;delete c;}
extern "C" void dkCmdBufClear(DkCmdBuf){}
extern "C" void dkCmdBufAddMemory(DkCmdBuf,DkMemBlock m,std::uint32_t,std::uint32_t){assert(m);}
extern "C" void dkCmdBufBarrier(DkCmdBuf,DkBarrier,std::uint32_t){}
extern "C" DkCmdList dkCmdBufFinishList(DkCmdBuf){++finishes;if(lose_at_finish)queue.lost=true;return 1;}
extern "C" bool dkQueueIsInErrorState(DkQueue q){return q->lost;}
extern "C" void dkQueueSubmitCommands(DkQueue q,DkCmdList){++submits;if(q->lost)++known_lost_dispatch;if(lose_at_submit){q->lost=true;pending_staging=latest_staging;}}
extern "C" void dkQueueFlush(DkQueue q){if(q->lost)++known_lost_flush;if(lose_at_flush){q->lost=true;pending_staging=latest_staging;}}
extern "C" void dkQueueWaitIdle(DkQueue q){++waits;if(q->lost)++known_lost_wait;if(lose_at_wait){q->lost=true;pending_staging=latest_staging;}}
extern "C" void dkQueueDestroy(DkQueue q){q->closed=true;}
extern "C" void dkImageLayoutInitialize(DkImageLayout* out,const DkImageLayoutMaker* maker){
    static_assert(sizeof(*maker)<=sizeof(*out));++layouts;std::memset(out,0,sizeof(*out));std::memcpy(out,maker,sizeof(*maker));
}
extern "C" std::uint64_t dkImageLayoutGetSize(const DkImageLayout* layout){DkImageLayoutMaker m;std::memcpy(&m,layout,sizeof(m));return std::uint64_t(m.dimensions[0])*m.dimensions[1]*4;}
extern "C" std::uint32_t dkImageLayoutGetAlignment(const DkImageLayout*){return 256;}
extern "C" void dkImageInitialize(DkImage* out,const DkImageLayout*,DkMemBlock m,std::uint32_t){assert(m);std::memset(out,0,sizeof(*out));}
extern "C" void dkImageDescriptorInitialize(DkImageDescriptor* out,const DkImageView*,bool,bool){std::memset(out,0,sizeof(*out));}
extern "C" void dkCmdBufCopyBufferToImage(DkCmdBuf,const DkCopyBuf*,const DkImageView*,const DkImageRect*,std::uint32_t){++copies;}
extern "C" void dkCmdBufCopyImageToBuffer(DkCmdBuf,const DkImageView*,const DkImageRect*,const DkCopyBuf*,std::uint32_t){++copies;}
nt::Texture Payload(){nt::Texture t;t.format=21;t.pool=1;t.levels.push_back({4,4,16,4,std::vector<std::byte>(64,std::byte{0x42})});t.levels.push_back({2,2,8,2,std::vector<std::byte>(16,std::byte{0x19})});return t;}
template<class F>bool Rejected(F f){try{f();}catch(const std::exception&){return true;}return false;}
void ClearFaults(){assert(!pending_staging || queue.closed);pending_staging=nullptr;queue.lost=queue.closed=false;lose_at_alloc=fail_at_alloc=0;lose_at_finish=lose_at_wait=lose_at_submit=lose_at_flush=false;}
int main(int argc,char** argv){
    bool baseline=argc>1;unsigned cases{};
    {
        nt::DekoTextureStore store(&device,&queue,1024*1024);auto payload=Payload();const auto free=store.Free();
        queue.lost=true;auto before=submits;auto alloc=allocations;
        assert(Rejected([&]{store.Upload(payload);}));assert(submits-before==(baseline?1u:0u));if(!baseline)assert(allocations==alloc);assert(store.Free()==free);++cases;
        auto count=layouts;assert(Rejected([&]{store.AllocateTarget(4,4,21);})==!baseline);if(!baseline)assert(layouts==count);++cases;
        ClearFaults();auto image=std::dynamic_pointer_cast<nt::DekoTextureStore::ResidentImage>(store.Upload(payload));assert(image);const auto retained=store.Free();
        // Known lost queue rejects read/write at entry in both versions.
        queue.lost=true;before=submits;alloc=allocations;
        assert(Rejected([&]{store.ReadImage(image->image,4,4,4);}));
        assert(Rejected([&]{store.WriteImage(image->image,4,4,4,payload.levels[0].bytes);}));assert(submits==before && allocations==alloc);cases+=2;
        // Queue error may become observable during staging or finishList.
        for(unsigned operation=0;operation<3;++operation)for(bool at_finish:{false,true}){
            ClearFaults();if(at_finish)lose_at_finish=true;else lose_at_alloc=allocations+1;before=submits;
            assert(Rejected([&]{if(operation==0)(void)store.Upload(payload);else if(operation==1)(void)store.ReadImage(image->image,4,4,4);else store.WriteImage(image->image,4,4,4,payload.levels[0].bytes);}));
            assert(submits-before==(baseline?1u:0u));assert(store.Free()==retained);++cases;
        }
        ClearFaults();fail_at_alloc=allocations+1;before=submits;
        assert(Rejected([&]{store.Upload(payload);}));assert(submits==before && store.Free()==retained);++cases;
        // Real native texture publication keeps the previous image on failure.
        ClearFaults();auto session=std::make_shared<nt::Session>();session->upload=[&](const auto& t){return store.Upload(t);};
        auto texture=std::make_shared<nt::Texture>(payload);texture->session=session;auto first=nt::Resident(texture);texture->dirty=true;
        queue.lost=true;before=submits;assert(Rejected([&]{nt::Resident(texture);}));assert(texture->image==first && texture->dirty);assert(submits-before==(baseline?1u:0u));++cases;
        ClearFaults();lose_at_wait=true;before=submits;auto waited=waits;const auto free_before=store.Free();
        assert(Rejected([&]{store.Upload(payload);}));assert(submits==before+1 && waits==waited+1 && store.Free()==free_before);++cases;
        if(!baseline)assert(store.HasFailedTransfer());
        // Borrowed queue must be closed before destroying any quarantine.
        assert(pending_staging);dkQueueDestroy(&queue);
    }
    for(bool on_submit:{true,false}){
        ClearFaults();nt::DekoTextureStore store(&device,&queue,1024*1024);auto payload=Payload();
        lose_at_submit=on_submit;lose_at_flush=!on_submit;
        assert(Rejected([&]{store.Upload(payload);}));if(!baseline)assert(store.HasFailedTransfer());assert(pending_staging);dkQueueDestroy(&queue);++cases;
    }
    const auto leaked=live_memory;assert(!live_commands);
    if(baseline){assert(known_lost_dispatch>0 && leaked>0);
        if(std::string(argv[1])=="baseline-leak"){
            // Remove test bookkeeping roots so LeakSanitizer can independently
            // observe actual allocations omitted by baseline SDK ownership.
            latest_staging=pending_staging=nullptr;std::fill(memory_registry.begin(),memory_registry.end(),nullptr);memory_registry.clear();
        }else{while(!memory_registry.empty())dkMemBlockDestroy(memory_registry.back());assert(!live_memory);}
    }else assert(!leaked && memory_registry.empty() && known_lost_dispatch==0 && known_lost_flush==0 && known_lost_wait==0);
    std::printf("TRANSFER_ADMISSION %s cases=%u known_lost_dispatch=%u staging_blocks_left_after_store=%u known_lost_flush=%u known_lost_wait=%u quarantine_until_channel_close production_store native_Resident previous_image_preserved allocation_rollback CPU_SDK_ENDPOINT_MODEL_ONLY NO_GPU_RECOVERY_NO_IMAGE_PROOF\n",baseline?"BASELINE_UNSAFE_DISPATCH_AND_STAGING_LEAK":"PASS",cases,known_lost_dispatch,leaked,known_lost_flush,known_lost_wait);
}
