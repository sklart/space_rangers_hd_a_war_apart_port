// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_draw_batch.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>
#include <algorithm>
namespace p=srhd_awa::matrixgame::platform3g;
namespace {
struct Owner {unsigned id;explicit Owner(unsigned value):id(value){++live;}~Owner(){--live;}static inline unsigned live{};};
struct Driver {
 p::NativeFrameScope& frame;p::NativeDrawBatch& budget;
 std::vector<std::unique_ptr<Owner>> pending;std::vector<unsigned> completed;
 unsigned submissions{},releases{},fail{};bool idle{};
 void SubmitAndWait(){
  frame.RequireBatch();assert(!idle && pending.size()==budget.Draws());++submissions;
  assert(Owner::live==pending.size());
  if(fail==1)throw std::runtime_error("injected submit failure");
  if(fail==2)throw std::runtime_error("injected completion failure");
  idle=true;
 }
 void ReleaseCompleted()noexcept{
  assert(idle && Owner::live==pending.size());
  for(const auto& packet:pending)completed.push_back(packet->id);
  pending.clear();idle=false;++releases;
 }
 void Add(unsigned id,std::uint64_t bytes){
  if(budget.NeedsDrain(bytes))p::DrainNativeDrawBatch(frame,budget,*this);
  pending.push_back(std::make_unique<Owner>(id));budget.Add(bytes);
 }
};
template<class F>void Fails(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}assert(failed);}
}
int main(int argc,char** argv){
 assert(argc==3);unsigned frames{},split_frames{},peak{},maximum_batch{};std::uint64_t total{},submissions{};
 for(int arg=1;arg<argc;++arg){std::ifstream input(argv[arg]);assert(input);std::string line;std::getline(input,line);if(!line.empty() && line.back()=='\r')line.pop_back();assert(line=="frame,all_draws,primary_draws,indexed_draws");
  while(std::getline(input,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);unsigned index{},all{},primary{},indexed{};row>>index>>all>>primary>>indexed;assert(row && primary && primary<=all && indexed<=all);
   p::NativeFrameScope frame;p::NativeDrawBatch budget;Driver driver{frame,budget};driver.completed.reserve(primary);
   frame.Acquire();frame.BeginScene();
   // Only measured draw counts are replayed here, not original geometry or GPU
   // bytes. Fixed small uploads isolate the production packet-count boundary.
   for(unsigned i=0;i<primary;++i){driver.Add(i,4096);maximum_batch=std::max(maximum_batch,unsigned(budget.Draws()));frame.RequireDraw();}
   Fails([&]{frame.RequirePresent();});
   frame.EndScene();p::DrainNativeDrawBatch(frame,budget,driver);frame.Submitted();frame.Presented();
   assert(!frame && !budget.Draws() && !budget.Bytes() && !Owner::live && driver.completed.size()==primary);
   for(unsigned i=0;i<primary;++i)assert(driver.completed[i]==i);
   assert(driver.submissions==(primary+p::NativeDrawBatch::DrawLimit-1)/p::NativeDrawBatch::DrawLimit);
   assert(driver.releases==driver.submissions);submissions+=driver.submissions;split_frames+=driver.submissions>1;total+=primary;peak=std::max(peak,primary);++frames;
  }
 }
 assert(frames==92 && split_frames==40 && peak==4038 && maximum_batch==2048);
 // Byte quota, exact boundary and oversize preflight must not mutate existing
 // owners/counters or start a queue operation.
 {
  p::NativeFrameScope frame;p::NativeDrawBatch budget(8,16);Driver driver{frame,budget};driver.completed.reserve(8);frame.Acquire();frame.BeginScene();
  driver.Add(0,8);driver.Add(1,8);assert(budget.NeedsDrain(1));
  const auto live=Owner::live;Fails([&]{budget.NeedsDrain(17);});assert(Owner::live==live && !driver.submissions && budget.Bytes()==16);
  driver.Add(2,8);assert(driver.completed==std::vector<unsigned>({0,1}) && budget.Draws()==1 && budget.Bytes()==8);frame.RequireDraw();
  frame.EndScene();p::DrainNativeDrawBatch(frame,budget,driver);frame.Submitted();frame.Presented();assert(!Owner::live);
 }
 for(unsigned failure:{1u,2u}){
  p::NativeFrameScope frame;p::NativeDrawBatch budget(1,16);Driver driver{frame,budget};driver.completed.reserve(2);frame.Acquire();frame.BeginScene();driver.Add(7,8);driver.fail=failure;
  Fails([&]{driver.Add(8,8);});assert(budget.Draws()==1 && budget.Bytes()==8 && driver.releases==0 && Owner::live==1 && driver.pending[0]->id==7);frame.RequireDraw();
  // No resubmit after a failed completion: model a separately established idle
  // during teardown, then release. This does not prove GPU queue-loss recovery.
  driver.idle=true;driver.ReleaseCompleted();budget.Retired();frame.Abort();assert(!Owner::live);
 }
 {
  p::NativeFrameScope frame;p::NativeDrawBatch budget;Driver driver{frame,budget};
  Fails([&]{p::DrainNativeDrawBatch(frame,budget,driver);});assert(!driver.submissions);
  frame.Acquire();driver.completed.reserve(1);driver.Add(0,1);p::DrainNativeDrawBatch(frame,budget,driver);frame.RequirePresent(); // Cursor/compositor phase.
  frame.Submitted();Fails([&]{p::DrainNativeDrawBatch(frame,budget,driver);});assert(driver.submissions==1);frame.Presented();
 }
 Fails([]{p::NativeDrawBatch invalid(0,16);});Fails([]{p::NativeDrawBatch invalid(1,0);});
 std::printf("native batch policy PASS original_frames=%u primary_draws=%llu split_frames=%u max_frame=%u max_batch=%u submissions=%llu order_exact owner_release_after_completion failure_retains_owners byte_quota scene_preserved CPU_QUEUE_MODEL_ONLY NO_SWITCH_GPU\n",frames,static_cast<unsigned long long>(total),split_frames,peak,maximum_batch,static_cast<unsigned long long>(submissions));
}
