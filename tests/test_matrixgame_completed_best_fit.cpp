// Production recycler/planner/budget; CPU owner endpoints, not GPU allocation
// latency. Real upload bytes are checked separately by test_draw_upload.
#ifdef NDEBUG
#error Reuse checks require assertions
#endif
#include "completed_block_pool.hpp"
#include "native_upload_reuse.hpp"
#include "native_draw_batch.hpp"
#include "baseline-exact-pool.inc"
#include <array>
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
namespace p=srhd_awa::matrixgame::platform3g;
struct Block {
 std::unique_ptr<std::array<std::uint64_t,2>> payload;std::uint64_t bytes{},id{};
 static inline std::uint64_t allocations{},live{},size_reads{};
 Block()=default;explicit Block(std::uint64_t n):payload(std::make_unique<std::array<std::uint64_t,2>>()),bytes(n),id(++allocations){++live;}
 Block(Block&& b)noexcept:payload(std::move(b.payload)),bytes(b.bytes),id(b.id){b.bytes=b.id=0;}
 Block& operator=(Block&& b)noexcept{if(payload)--live;payload=std::move(b.payload);bytes=b.bytes;id=b.id;b.bytes=b.id=0;return *this;}
 ~Block(){if(payload)--live;}explicit operator bool()const{return bool(payload);}std::uint64_t getSize()const{++size_reads;return bytes;}
};
template<class Pool,bool Best>struct Driver {
 Pool pool;p::NativeFrameScope frame;p::NativeDrawBatch batch;std::uint64_t quota;
 struct Packet {Block upload,uniform;std::uint64_t sequence,requested;};std::vector<Packet> pending;
 std::uint64_t draws{},expected{},submissions{},padding{},peak_bytes{};bool fail{},done{};p::UploadReusePlan last_plan{};
 explicit Driver(std::uint64_t limit=p::NativeDrawBatch::ByteLimit):batch(64,limit),quota(limit){pending.reserve(64);}
 auto Plan(std::uint64_t bytes){if constexpr(Best)return p::PlanUploadReuse(pool,bytes,4096,quota);else return p::UploadReusePlan{bytes,bytes+4096};}
 Block Memory(std::uint64_t requested,std::uint64_t maximum){Block b;if constexpr(Best)b=pool.TakeAtLeast(requested,maximum);else b=pool.Take(requested);return b?std::move(b):Block(requested);}
 void Add(std::uint64_t requested){auto plan=Plan(requested);if(batch.NeedsDrain(plan.total)){p::DrainNativeDrawBatch(frame,batch,*this);plan=Plan(requested);}
  Packet q{Memory(requested,plan.capacity),Memory(4096,4096),draws++,requested};assert(q.upload.bytes==plan.capacity && q.uniform.bytes==4096);
  assert(q.upload.bytes>=requested && q.upload.bytes<=2*requested);
  for(const auto& live:pending)assert(live.upload.id!=q.upload.id && live.uniform.id!=q.uniform.id && live.upload.id!=q.uniform.id && live.uniform.id!=q.upload.id);
  (*q.upload.payload)[0]=q.sequence;(*q.upload.payload)[1]=requested;(*q.uniform.payload)[0]=~q.sequence;pending.push_back(std::move(q));
  batch.Add(plan.total);assert(batch.Bytes()<=quota && batch.Draws()<=64);last_plan=plan;padding+=plan.capacity-requested;peak_bytes=std::max(peak_bytes,batch.Bytes());
 }
 void SubmitAndWait(){++submissions;if(fail)throw std::runtime_error("CPU completion failure");
  for(const auto& q:pending){assert(q.sequence==expected++ && (*q.upload.payload)[0]==q.sequence && (*q.upload.payload)[1]==q.requested && (*q.uniform.payload)[0]==~q.sequence);}done=true;}
 void ReleaseCompleted()noexcept{assert(done);for(auto& q:pending){pool.Put(std::move(q.upload));pool.Put(std::move(q.uniform));}pending.clear();done=false;assert(pool.Bytes()<=16ull*1024*1024);}
 void Finish(){frame.EndScene();p::DrainNativeDrawBatch(frame,batch,*this);frame.Submitted();frame.Presented();}
};
struct Stats {std::uint64_t allocations,reads,draws,submissions,padding,peak;};
template<class Pool,bool Best>Stats Sweep(){
 assert(!Block::live);Block::allocations=Block::size_reads=0;Driver<Pool,Best> d;
 for(unsigned phase=0;phase<512;++phase){d.frame.Acquire();d.frame.BeginScene();for(unsigned draw=0;draw<64;++draw)d.Add(4096ull*(3+(draw*7)%16+phase%5));d.Finish();}
 assert(d.expected==32768 && d.submissions==512);return {Block::allocations,Block::size_reads,d.expected,d.submissions,d.padding,d.peak_bytes};
}
template<class F>void Reject(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}assert(caught);}
int main(){
 using Pool=p::CompletedBlockPool<Block>;auto old=Sweep<p::FrozenExactPool<Block>,false>();auto next=Sweep<Pool,true>();assert(!Block::live);
 assert(next.allocations<old.allocations && next.reads<old.reads && next.draws==old.draws && next.submissions==old.submissions);
 {Pool pool;pool.Put(Block(16384));pool.Put(Block(32768));const auto reads=Block::size_reads;
  for(unsigned i=0;i<100;++i)assert(pool.BestFitSize(12288,24576)==16384);auto a=pool.TakeAtLeast(12288,24576);assert(a.bytes==16384 && pool.Bytes()==32768 && pool.larger_hits==1);
  assert(!pool.Take(4096));assert(Block::size_reads==reads);assert(!pool.BestFitSize(0,UINT64_MAX));assert(!pool.BestFitSize(4096,1));
  auto huge=p::PlanUploadReuse(pool,p::NativeDrawBatch::ByteLimit-4096,4096,p::NativeDrawBatch::ByteLimit);assert(huge.total==p::NativeDrawBatch::ByteLimit);
  Reject([&]{p::PlanUploadReuse(pool,0,4096,65536);});Reject([&]{p::PlanUploadReuse(pool,65536,4096,65536);});Reject([&]{p::PlanUploadReuse(pool,1,0,65536);});Reject([&]{p::PlanUploadReuse(pool,UINT64_MAX,1,UINT64_MAX);});
 }
 {Driver<Pool,true> d(48*1024);d.frame.Acquire();d.frame.BeginScene();d.Add(32768);d.Add(24576);assert(d.submissions==1 && d.last_plan.capacity==32768 && d.batch.Bytes()==36864);d.frame.RequireDraw();d.Finish();}
 {Driver<Pool,true> d;d.frame.Acquire();d.frame.BeginScene();d.Add(16384);d.fail=true;Reject([&]{p::DrainNativeDrawBatch(d.frame,d.batch,d);});assert(d.pending.size()==1 && !d.pool.Bytes() && d.batch.Draws()==1);}
 assert(!Block::live);
 std::printf("BEST_FIT PASS draws=%llu submissions=%llu exact_allocations=%llu fitted_allocations=%llu exact_size_reads=%llu fitted_size_reads=%llu padding_bytes=%llu peak_live_bytes=%llu getter_free_lookup physical_quota_replan exact_uniforms bounded_2x_payload order_completion_failure_retains CPU_MODEL_NOT_SWITCH\n",(unsigned long long)next.draws,(unsigned long long)next.submissions,(unsigned long long)old.allocations,(unsigned long long)next.allocations,(unsigned long long)old.reads,(unsigned long long)next.reads,(unsigned long long)next.padding,(unsigned long long)next.peak);
}
