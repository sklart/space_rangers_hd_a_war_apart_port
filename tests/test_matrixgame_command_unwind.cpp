// Actual production member declarations, SDK UniqueHandle and command-page
// owner. The SDK C endpoints below only observe CPU ownership, not a GPU UAF.
#include <deko3d.hpp>
#include "native_command_memory.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <stdexcept>
namespace dk::detail {
struct MemBlock {unsigned id;};
struct CmdBuf {std::vector<DkMemBlock> references;};
}
std::vector<DkCmdBuf> recorders;
unsigned violations{},live_memory{},created_memory{},destroyed_recorders{},stage{},fail_at{};
void Checkpoint(){if(++stage==fail_at)throw std::bad_alloc();}
extern "C" void dkMemBlockDestroy(DkMemBlock memory){
    for(auto recorder:recorders)for(auto reference:recorder->references)if(reference==memory)++violations;
    assert(live_memory);--live_memory;delete memory;
}
extern "C" void dkCmdBufDestroy(DkCmdBuf recorder){
    auto pos=std::find(recorders.begin(),recorders.end(),recorder);assert(pos!=recorders.end());recorders.erase(pos);++destroyed_recorders;delete recorder;
}
extern "C" void dkCmdBufAddMemory(DkCmdBuf recorder,DkMemBlock memory,std::uint32_t,std::uint32_t){
    assert(recorder && memory);recorder->references.push_back(memory);
}
using namespace srhd_awa::matrixgame::platform3g;
struct Impl {
#include "command-owner-fields.inc"
    dk::UniqueMemBlock Allocate(std::uint32_t){
        Checkpoint();auto memory=new dk::detail::MemBlock{++created_memory};++live_memory;return dk::MemBlock{memory};
    }
    Impl(){
        command_memory=Allocate(2*1024*1024);command_discard=Allocate(16*1024*1024);
        Checkpoint();auto recorder=new dk::detail::CmdBuf;recorders.push_back(recorder);commands=dk::CmdBuf{recorder};
        commands.addMemory(command_memory,0,2*1024*1024);Checkpoint();
        command_pages.Refill(commands,4096);command_pages.Check();Checkpoint();
        command_pages.Refill(commands,4096);command_pages.Check();Checkpoint();
    }
    // Like the real normal teardown, explicit reset is safe. Constructor unwind
    // skips this body, so the generated declaration order is decisive there.
    ~Impl(){commands=nullptr;}
};
int main(int argc,char**){
    const bool baseline=argc>1;unsigned cases{};
    for(unsigned failure=0;failure<=8;++failure){
        stage=0;fail_at=failure;try{Impl value;}catch(const std::bad_alloc&){}
        assert(!live_memory && recorders.empty());++cases;
    }
    if(baseline)assert(violations>0);else assert(violations==0);
    std::printf("COMMAND_UNWIND %s cases=%u released_backing_while_recorder_retained=%u SDK_UniqueHandle production_fields real_command_pages CPU_OWNERSHIP_MODEL_ONLY NO_GPU_UAF_CLAIM\n",baseline?"BASELINE_INVARIANT_VIOLATED":"PASS",cases,violations);
}
