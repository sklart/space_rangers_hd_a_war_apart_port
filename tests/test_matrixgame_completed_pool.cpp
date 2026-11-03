// Real completion/budget/recycler code. CPU blocks model GPU owners explicitly.
#include "completed_block_pool.hpp"
#include "native_draw_batch.hpp"
#include "native_profile.hpp"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
namespace p=srhd_awa::matrixgame::platform3g;
struct Block {
 std::unique_ptr<std::uint64_t[]> data;std::uint64_t bytes{},id{};static inline std::uint64_t allocations{},live{};
 Block()=default;explicit Block(unsigned n):data(std::make_unique<std::uint64_t[]>(n/8)),bytes(n),id(++allocations){++live;}
 Block(Block&& b)noexcept:data(std::move(b.data)),bytes(b.bytes),id(b.id){b.bytes=b.id=0;}
 Block& operator=(Block&& b)noexcept{if(data)--live;data=std::move(b.data);bytes=b.bytes;id=b.id;b.bytes=b.id=0;return *this;}
 ~Block(){if(data)--live;}explicit operator bool()const{return bool(data);}std::uint64_t getSize()const{return bytes;}
};
struct Driver {
 p::CompletedBlockPool<Block> pool;p::NativeFrameScope frame;p::NativeDrawBatch batch;
 struct Packet {Block upload,uniform;std::uint64_t sequence;};std::vector<Packet> pending;
 std::uint64_t expected{},draws{},submits{};bool fail{},completed{};
 Driver(){pending.reserve(64);}
 Block Memory(unsigned size){auto b=pool.Take(size);return b?std::move(b):Block(size);}
 void Add(){if(batch.NeedsDrain(20480))p::DrainNativeDrawBatch(frame,batch,*this);
  Packet packet{Memory(16384),Memory(4096),draws++};
  for(const auto& other:pending)assert(other.upload.id!=packet.upload.id && other.uniform.id!=packet.uniform.id);
  packet.upload.data[0]=packet.sequence;packet.uniform.data[0]=~packet.sequence;pending.push_back(std::move(packet));batch.Add(20480);
 }
 void SubmitAndWait(){++submits;if(fail)throw std::runtime_error("CPU queue completion failure");
  for(const auto& q:pending){assert(q.sequence==expected++);assert(q.upload.data[0]==q.sequence && q.uniform.data[0]==~q.sequence);}completed=true;
 }
 void ReleaseCompleted()noexcept{assert(completed);for(auto& q:pending){pool.Put(std::move(q.upload));pool.Put(std::move(q.uniform));}pending.clear();completed=false;}
};
int main(int argc,char** argv){
 assert(argc==3);std::uint64_t total{},frames{};Driver driver;
 for(int arg=1;arg<argc;++arg){std::ifstream f(argv[arg]);assert(f);std::string line;std::getline(f,line);
  while(std::getline(f,line)){std::replace(line.begin(),line.end(),',',' ');std::istringstream row(line);unsigned index,all,draws,indexed;row>>index>>all>>draws>>indexed;assert(row && draws);
   driver.frame.Acquire();driver.frame.BeginScene();for(unsigned i=0;i<draws;++i)driver.Add();
   driver.frame.EndScene();p::DrainNativeDrawBatch(driver.frame,driver.batch,driver);driver.frame.Submitted();driver.frame.Presented();total+=draws;++frames;
  }
 }
 assert(total==208808 && frames==92 && driver.expected==total && driver.submits==3310);
 assert(Block::allocations==128 && driver.pool.hits==2*total-128 && driver.pool.misses==128);
 const auto bytes=driver.pool.Bytes();assert(bytes==64*20480);driver.pool.Clear();assert(!Block::live);
 {Driver failure;failure.frame.Acquire();failure.frame.BeginScene();failure.Add();failure.fail=true;bool caught=false;
  try{p::DrainNativeDrawBatch(failure.frame,failure.batch,failure);}catch(...){caught=true;}
  assert(caught && failure.pending.size()==1 && failure.batch.Draws()==1 && !failure.pool.Bytes() && Block::live==2);
 }assert(!Block::live);
 {p::CompletedBlockPool<Block,2> pool(8192);pool.Put(Block(4096));pool.Put(Block(4096));pool.Put(Block(8192));assert(pool.Bytes()==8192 && Block::live==1);assert(!pool.Take(4096));auto b=pool.Take(8192);assert(b && !pool.Bytes());pool.Put(Block(16384));assert(!pool.Bytes());}
 assert(!Block::live);
 unsigned sampled{};for(unsigned i=1;i<=591;++i)sampled+=p::SamplePressure(i);assert(sampled==11);
 p::ProfileTotals timing;{p::ProfileScope s(timing);s.Stop();s.Stop();}assert(timing.calls==1);
 std::printf("COMPLETED_POOL PASS frames=92 draws=208808 submissions=3310 baseline_blocks=417616 pooled_blocks=128 reuse_hits=417488 bytes=1310720 exact_order independent_live_payload completion_failure_retains_owners bounded_eviction oversized_drop telemetry_591_to_11 no_early_reuse CPU_MODEL_NOT_GPU\n");
}
