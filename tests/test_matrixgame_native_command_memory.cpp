// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_command_memory.hpp"
#include "native_draw_batch.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <memory>
#include <algorithm>
namespace p=srhd_awa::matrixgame::platform3g;
namespace {
struct State;
struct Page {
 State& state;unsigned id;std::vector<std::byte> storage;
 Page(State& s,unsigned value,unsigned bytes):state(s),id(value),storage(bytes){}
 ~Page();
};
struct State {
 std::unique_ptr<Page> base,discard;Page* current{};
 std::array<Page*,32> referenced{};unsigned refs{},offset{},next_id{2},allocations{},attaches{},discards{},destroyed{},finishes{},submits{},clears{},fail_null{},fail_throw{};
 bool gpu_live{};
 State():base(std::make_unique<Page>(*this,0,2*1024*1024)),discard(std::make_unique<Page>(*this,1,16*1024*1024)){current=base.get();referenced[refs++]=current;}
 ~State(){assert(!gpu_live);current=nullptr;refs=0;}
 void Attach(Page* page){assert(refs<referenced.size());current=page;offset=0;referenced[refs++]=page;}
 void ClearAndRebind()noexcept{assert(!gpu_live);++clears;refs=0;current=base.get();offset=0;referenced[refs++]=current;}
 void DestroyCommand()noexcept{assert(!gpu_live);current=nullptr;refs=0;}
};
Page::~Page(){if(id<2)return;assert(!state.gpu_live && state.current!=this);for(unsigned i=0;i<state.refs;++i)assert(state.referenced[i]!=this);++state.destroyed;}
struct Driver {
 using Command=unsigned;using Block=std::unique_ptr<Page>;State& state;
 Block Allocate(unsigned bytes){++state.allocations;if(state.allocations==state.fail_null)return {};if(state.allocations==state.fail_throw)throw std::bad_alloc();return std::make_unique<Page>(state,state.next_id++,bytes);}
 void Attach(Command command,const Block& block,unsigned bytes)noexcept{assert(command==7 && block && block->storage.size()==bytes);++state.attaches;state.Attach(block.get());}
 void AttachDiscard(Command command)noexcept{assert(command==7);++state.discards;state.Attach(state.discard.get());}
 [[noreturn]] void RejectRequest(std::uint64_t)noexcept{std::terminate();}
};
using Arena=p::NativeCommandMemory<Driver>;
template<class E,class F>void Throws(F action){bool threw=false;try{action();}catch(const E&){threw=true;}assert(threw);}
void Write(State& state,Arena& arena,unsigned bytes,std::byte marker){
 // Explicit CPU writer model of the documented add-memory request protocol.
 // This is not the deko encoder, GPU submission or a hardware overflow test.
 if(state.current->storage.size()-state.offset<bytes)arena.Refill(7,bytes);
 assert(state.current->storage.size()-state.offset>=bytes);
 std::fill_n(state.current->storage.begin()+state.offset,bytes,marker);state.offset+=bytes;
}
void Submit(State& state,Arena& arena){
 const auto list=arena.FinishForSubmission([&]{++state.finishes;return 17;});assert(list==17);++state.submits;state.gpu_live=true;
}
void CompleteAndRetire(State& state,Arena& arena){state.gpu_live=false;arena.Retire([&]()noexcept{state.ClearAndRebind();});assert(arena.Pages()==0 && arena.Bytes()==Arena::BaseBytes && arena.Failure()==p::CommandMemoryFailure::None);}
struct PipelineDriver {
 State& state;Arena& arena;bool fail_completion{};unsigned releases{};
 void SubmitAndWait(){Submit(state,arena);if(fail_completion)throw std::runtime_error("injected queue completion error");state.gpu_live=false;}
 void ReleaseCompleted()noexcept{assert(!state.gpu_live);arena.Retire([&]()noexcept{state.ClearAndRebind();});++releases;}
};
}
int main(){
 assert(Arena::PageSize(1)==Arena::BaseBytes && Arena::PageSize(Arena::BaseBytes+1)==Arena::BaseBytes+Arena::Alignment && Arena::PageSize(Arena::QuotaBytes)==Arena::QuotaBytes);
 Throws<std::length_error>([]{Arena::PageSize(0);});Throws<std::length_error>([]{Arena::PageSize(UINT64_MAX);});
 unsigned growth{},failure_cases{};
 {
  State state;Arena arena(Driver{state});
  for(unsigned i=0;i<10;++i)Write(state,arena,1024*1024,std::byte(i+1));
  assert(arena.Pages()==4 && arena.Bytes()==10*1024*1024 && state.discards==0);growth=state.attaches;
  for(unsigned page=0;page<state.refs;++page){const auto* value=state.referenced[page];assert(value->storage.front()==std::byte(2*page+1) && value->storage.back()==std::byte(2*page+2));}
  Submit(state,arena);assert(state.destroyed==0 && arena.Pages()==4); // All pages retained through simulated queue work.
  CompleteAndRetire(state,arena);assert(state.destroyed==4 && state.current==state.base.get());
  Write(state,arena,1024*1024,std::byte{42});arena.Check();assert(state.current==state.base.get());
 }
 for(bool throws:{false,true}){
  State state;Arena arena(Driver{state});Write(state,arena,Arena::BaseBytes,std::byte{11});Write(state,arena,Arena::BaseBytes,std::byte{22});
  const auto base=state.base->storage,spill=state.current->storage;const auto old_destroyed=state.destroyed;
  if(throws)state.fail_throw=state.allocations+1;else state.fail_null=state.allocations+1;
  Write(state,arena,4096,std::byte{33});assert(arena.Failure()==p::CommandMemoryFailure::Allocation && state.current==state.discard.get());
  const auto allocations=state.allocations;arena.Refill(7,16);assert(state.allocations==allocations && state.discards==2);
  assert(state.base->storage==base && state.referenced[1]->storage==spill && state.destroyed==old_destroyed);
  Throws<std::bad_alloc>([&]{Submit(state,arena);});assert(state.finishes==0 && state.submits==0 && arena.Pages()==1);
  CompleteAndRetire(state,arena);assert(state.destroyed==1);arena.Refill(7,4096);arena.Check();assert(arena.Pages()==1);
  CompleteAndRetire(state,arena);++failure_cases;
 }
 {
  State state;Arena arena(Driver{state});
  for(unsigned i=0;i<8;++i)Write(state,arena,Arena::BaseBytes,std::byte(i));
  assert(arena.Bytes()==Arena::QuotaBytes && arena.Pages()==7);const auto allocations=state.allocations;
  Write(state,arena,1,std::byte{99});assert(arena.Failure()==p::CommandMemoryFailure::Quota && state.allocations==allocations);
  Throws<std::length_error>([&]{Submit(state,arena);});assert(state.finishes==0 && state.submits==0 && state.destroyed==0);
  arena.Refill(7,Arena::QuotaBytes);assert(arena.Failure()==p::CommandMemoryFailure::Quota && state.current->storage.size()>=Arena::QuotaBytes);
  CompleteAndRetire(state,arena);++failure_cases;
 }
 {
  State state;Arena arena(Driver{state});state.fail_null=1;
  // A failure arising inside finish itself must also block the returned list.
  Throws<std::bad_alloc>([&]{arena.FinishForSubmission([&]{++state.finishes;arena.Refill(7,4);return 17;});});
  assert(state.finishes==1 && state.submits==0);CompleteAndRetire(state,arena);++failure_cases;
 }
 {
  State state;auto arena=std::make_unique<Arena>(Driver{state});arena->Refill(7,4);assert(arena->Pages()==1);
  state.DestroyCommand();arena.reset();assert(state.destroyed==1); // Real Impl destroys CmdBuf before page owners.
 }
 for(unsigned mode:{0u,1u,2u}){
  State state;Arena arena(Driver{state});p::NativeFrameScope frame;p::NativeDrawBatch batch;PipelineDriver driver{state,arena};
  frame.Acquire();frame.BeginScene();
  for(unsigned i=0;i<7;++i)Write(state,arena,Arena::BaseBytes,std::byte(i));
  batch.Add(4096);assert(arena.NeedsDrain() && arena.Pages()==6 && state.destroyed==0);
  if(mode==1){state.fail_null=state.allocations+1;arena.Refill(7,4);}
  driver.fail_completion=mode==2;
  if(mode==0){
   p::DrainNativeDrawBatch(frame,batch,driver);frame.RequireDraw();
   assert(!arena.NeedsDrain() && !batch.Draws() && arena.Pages()==0 && state.destroyed==6 && driver.releases==1 && state.submits==1);
   frame.EndScene();frame.RequirePresent();frame.Abort();
  }else{
   Throws<std::exception>([&]{p::DrainNativeDrawBatch(frame,batch,driver);});frame.RequireDraw();
   assert(arena.Pages()==6 && batch.Draws()==1 && state.destroyed==0 && driver.releases==0);
   assert(mode==1?state.submits==0:(state.submits==1 && state.gpu_live));
   // Teardown only after a separately established idle; no retry/resubmit.
   state.gpu_live=false;driver.ReleaseCompleted();batch.Retired();frame.Abort();++failure_cases;
  }
 }
 {
  State state;Arena arena(Driver{state});p::NativeFrameScope frame;p::NativeDrawBatch batch;PipelineDriver driver{state,arena};frame.Acquire();
  for(unsigned i=0;i<7;++i)Write(state,arena,Arena::BaseBytes,std::byte(i));
  assert(arena.NeedsDrain() && batch.Draws()==0);p::DrainNativeDrawBatch(frame,batch,driver);
  frame.RequireClear();frame.RequirePresent();assert(arena.Pages()==0 && state.destroyed==6 && driver.releases==1);frame.Abort();
 }
 std::printf("native command memory PASS growth_pages=%u failure_cases=%u null_and_throw_OOM quota_latch finish_time_failure no_failed_submit clear_rebind_before_free teardown_order exact_bytes batch_composition command_pressure_drain clear_pressure_drain completion_failure_retains_pages CPU_WRITER_QUEUE_MODEL_ONLY NO_SWITCH_GPU\n",growth,failure_cases);
}
