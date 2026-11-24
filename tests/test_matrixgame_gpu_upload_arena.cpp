#include "native_gpu_upload_arena.hpp"
#include "native_draw_upload.hpp"
#include "native_draw_batch.hpp"
#include "native_sampler_set_cache.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
namespace p=srhd_awa::matrixgame::platform3g;
namespace dk::detail {struct Device{};struct MemBlock{std::vector<std::byte> data;DkGpuAddr gpu;};}
dk::detail::Device device;
std::vector<DkMemBlock> live;
bool fail_allocate{};std::uint64_t maximum_allocation{};unsigned cpu_queries{},gpu_queries{};
extern "C" DkMemBlock dkMemBlockCreate(const DkMemBlockMaker* m){
 if(fail_allocate || (maximum_allocation && m->size>maximum_allocation))return nullptr;
 auto* b=new dk::detail::MemBlock{std::vector<std::byte>(m->size),0x10000000};live.push_back(b);return b;
}
extern "C" void dkMemBlockDestroy(DkMemBlock b){auto it=std::find(live.begin(),live.end(),b);assert(it!=live.end());live.erase(it);delete b;}
extern "C" void* dkMemBlockGetCpuAddr(DkMemBlock b){++cpu_queries;return b->data.data();}
extern "C" DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock b){++gpu_queries;return b->gpu;}
extern "C" std::uint32_t dkMemBlockGetSize(DkMemBlock b){return std::uint32_t(b->data.size());}
auto Allocate(std::uint64_t bytes){return dk::MemBlockMaker{&device,std::uint32_t(bytes)}.create();}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
struct Driver{
 p::NativeGpuUploadArena<dk::UniqueMemBlock>& arena;
 std::vector<p::NativeDrawUpload>& uploads;
 p::NativeSamplerSetCache& sampler_cache;
 bool fail_submit{},fail_wait{};unsigned retired{};
 void SubmitAndWait(){if(fail_submit)throw std::runtime_error("submit");if(fail_wait)throw std::runtime_error("wait");}
 void ReleaseCompleted()noexcept{uploads.clear();sampler_cache.Invalidate();auto b=arena.Retire();++retired;}
};
int main(){
 constexpr auto budget=p::NativeDrawBatch::ByteLimit;
 p::NativeGpuUploadArena<dk::UniqueMemBlock> arena;
 fail_allocate=true;Reject([&]{arena.Ensure(4096,budget,Allocate);});fail_allocate=false;assert(live.empty() && !arena.HasBlock());
 Reject([&]{arena.Ensure(budget+1,budget,Allocate);});assert(live.empty());
 maximum_allocation=arena.FallbackBytes;
 assert(arena.Ensure(4096,budget,Allocate) && arena.Capacity()==arena.FallbackBytes);
 {auto fallback=arena.Retire();fallback=nullptr;}assert(live.empty());
 assert(!arena.Ensure(4096,budget,Allocate,arena.FallbackBytes) && arena.Capacity()==arena.FallbackBytes);
 {auto small=arena.Retire();small=nullptr;}assert(live.empty());
 Reject([&]{arena.Ensure(arena.FallbackBytes+DK_MEMBLOCK_ALIGNMENT,budget,Allocate);});assert(!arena.HasBlock() && live.empty());
 maximum_allocation=0;
 arena.Ensure(4096,budget,Allocate);assert(live.size()==1 && arena.Capacity()==arena.DefaultBytes);
 const auto cpu_before=cpu_queries,gpu_before=gpu_queries;
 p::NativeDrawBatch batch{2048,budget};p::NativeFrameScope frame;frame.Acquire();frame.BeginScene();
 std::vector<p::NativeDrawUpload> uploads;std::vector<std::byte> vertices(192,std::byte{0x37}),indices;
 auto plan=p::NativeDrawUpload::PlanArena(vertices.size(),indices.size(),true);
 p::NativeSamplerSetCache sampler_cache;p::NativeSamplerSetCache::Key sampler_key{};std::uint64_t sampler_addr{};
 std::byte* first_sampler=nullptr;
 assert(plan.bytes<p::NativeDrawUpload::Plan(vertices.size(),indices.size(),true).bytes);
 for(unsigned i=0;i<2048;++i){auto slice=arena.Append(plan.bytes,DK_MEMBLOCK_ALIGNMENT);
  uploads.emplace_back(slice.cpu,slice.gpu,std::uint32_t(slice.bytes),vertices,indices,true);batch.Add(plan.bytes);
  assert(uploads.back().Vertices()==0x10000000+std::uint64_t(i)*plan.bytes);
  assert(std::equal(vertices.begin(),vertices.end(),slice.cpu));
  assert(std::equal(indices.begin(),indices.end(),slice.cpu+plan.indices));
  if(!sampler_cache.Find(sampler_key,sampler_addr)){
   DkSamplerDescriptor descriptor;std::memset(&descriptor,0x57,sizeof(descriptor));
   for(unsigned slot=0;slot<8;++slot)uploads.back().WriteSampler(slot,descriptor);
   sampler_addr=uploads.back().Samplers();first_sampler=slice.cpu+plan.samplers;sampler_cache.Publish(sampler_key,sampler_addr);
  }
  assert(sampler_addr==0x10000000+plan.samplers && first_sampler[0]==std::byte{0x57});
 }
 assert(live.size()==1 && batch.LimitsReached(plan.bytes)&p::NativeDrawBatch::DrawCount);
 assert(cpu_queries==cpu_before && gpu_queries==gpu_before);
 const auto old_used=arena.Used();Driver driver{arena,uploads,sampler_cache};driver.fail_submit=true;
 Reject([&]{p::DrainNativeDrawBatch(frame,batch,driver);});assert(arena.HasBlock() && arena.Used()==old_used && batch.Draws()==2048 && live.size()==1 && uploads.size()==2048);
 driver.fail_submit=false;driver.fail_wait=true;Reject([&]{p::DrainNativeDrawBatch(frame,batch,driver);});
 assert(arena.HasBlock() && arena.Used()==old_used && batch.Draws()==2048 && live.size()==1 && driver.retired==0 && uploads.size()==2048);
 assert(sampler_cache.Find(sampler_key,sampler_addr) && first_sampler[0]==std::byte{0x57});
 driver.fail_wait=false;p::DrainNativeDrawBatch(frame,batch,driver);
 assert(!arena.HasBlock() && !batch.Draws() && live.empty() && driver.retired==1 && !sampler_cache.Find(sampler_key,sampler_addr));
 indices.assign(12,std::byte{0x51});plan=p::NativeDrawUpload::PlanArena(vertices.size(),indices.size(),true);
 arena.Ensure(plan.bytes,budget,Allocate);auto indexed=arena.Append(plan.bytes,DK_MEMBLOCK_ALIGNMENT);
 {
  p::NativeDrawUpload upload{indexed.cpu,indexed.gpu,std::uint32_t(indexed.bytes),vertices,indices,true};
  assert(std::equal(indices.begin(),indices.end(),indexed.cpu+plan.indices));
  DkImageDescriptor image;std::memset(&image,0x75,sizeof(image));upload.WriteImage(3,image);
  assert(!std::memcmp(indexed.cpu+plan.images+3*sizeof(image),&image,sizeof(image)));
  auto moved=std::move(upload);Reject([&]{upload.Vertices();});assert(moved.Vertices()==indexed.gpu);
  Reject([&]{p::NativeDrawUpload bad{indexed.cpu,indexed.gpu+1,std::uint32_t(indexed.bytes),vertices,indices,true};});
  Reject([&]{p::NativeDrawUpload bad{indexed.cpu,indexed.gpu,1,vertices,indices,true};});
 }
 auto indexed_block=arena.Retire();indexed_block=nullptr;assert(live.empty());
 {
  const auto owned_plan=p::NativeDrawUpload::Plan(vertices.size(),indices.size(),true);
  auto owned=Allocate(owned_plan.bytes);p::NativeDrawUpload upload{std::move(owned),vertices,indices,true};
  auto moved=std::move(upload);Reject([&]{upload.Vertices();});assert(moved.StorageBytes()==owned_plan.bytes);
  auto returned=moved.TakeCompletedMemory();assert(!moved.StorageBytes());Reject([&]{moved.Vertices();});
 }
 assert(live.empty());
 unsigned layouts=0;
 for(unsigned v:{1u,4u,16u,192u,576u,4095u,4096u,4097u,8192u,65537u})
 for(unsigned n:{0u,2u,4u,6u,4095u,4096u,4097u,32769u}){
  std::vector<std::byte> vb(v),ib(n);for(unsigned i=0;i<v;++i)vb[i]=std::byte(i*7u+3u);for(unsigned i=0;i<n;++i)ib[i]=std::byte(i*13u+5u);
  const auto old=p::NativeDrawUpload::Plan(v,n,true),compact=p::NativeDrawUpload::PlanArena(v,n,true);
  assert(compact.bytes<=old.bytes && compact.bytes%DK_MEMBLOCK_ALIGNMENT==0);
  assert(compact.indices>=v && compact.samplers>=compact.indices+n);
  assert(compact.images>=compact.samplers+8*sizeof(DkSamplerDescriptor));
  assert(compact.uniforms>=compact.images+8*sizeof(DkImageDescriptor));
  assert(compact.bytes>=compact.uniforms+p::NativeDrawUpload::ArenaUniformBytes);
  arena.Ensure(compact.bytes,budget,Allocate);auto s=arena.Append(compact.bytes,DK_MEMBLOCK_ALIGNMENT);
  {
   p::NativeDrawUpload actual{s.cpu,s.gpu,std::uint32_t(s.bytes),vb,ib,true};
   assert(actual.Vertices()==s.gpu && actual.Indices()==s.gpu+compact.indices);
   assert(actual.Samplers()%DK_SAMPLER_DESCRIPTOR_ALIGNMENT==0 && actual.Images()%DK_IMAGE_DESCRIPTOR_ALIGNMENT==0);
   assert(actual.UniformAddress()%DK_UNIFORM_BUF_ALIGNMENT==0 && actual.Indices()%4==0);
   assert(std::equal(vb.begin(),vb.end(),s.cpu+compact.vertices));assert(std::equal(ib.begin(),ib.end(),s.cpu+compact.indices));
   for(unsigned slot=0;slot<8;++slot){DkImageDescriptor image;std::memset(&image,int(slot+1),sizeof(image));actual.WriteImage(slot,image);assert(!std::memcmp(s.cpu+compact.images+slot*sizeof(image),&image,sizeof(image)));}
   std::fill(actual.UniformStorage().begin(),actual.UniformStorage().end(),std::byte{0x72});
   assert(s.cpu[compact.uniforms]==std::byte{0x72} && s.cpu[compact.uniforms+p::NativeDrawUpload::ArenaUniformBytes-1]==std::byte{0x72});
  }
  auto completed=arena.Retire();completed=nullptr;assert(live.empty());++layouts;
 }
 assert(layouts==80);
 {
  std::vector<std::byte> vb(192);auto layout=p::NativeDrawUpload::PlanArena(vb.size(),0,true);
  arena.Ensure(layout.bytes,budget,Allocate);auto s=arena.Append(layout.bytes,DK_MEMBLOCK_ALIGNMENT);
  std::memset(s.cpu,0xa5,s.bytes);
  {p::NativeDrawUpload upload{s.cpu,s.gpu,std::uint32_t(s.bytes),vb,{},true,false};
   DkImageDescriptor zero{},image;std::memset(&image,0x61,sizeof(image));
   for(unsigned mask=0;mask<256;++mask){
    std::memset(s.cpu+layout.images,0xa5,8*sizeof(image));
    if(!mask)upload.ClearImages();
    else for(unsigned slot=0;slot<8;++slot)upload.WriteImage(slot,mask&(1u<<slot)?image:zero);
    for(unsigned slot=0;slot<8;++slot)assert(!std::memcmp(s.cpu+layout.images+slot*sizeof(image),mask&(1u<<slot)?&image:&zero,sizeof(image)));
   }
   upload.ClearImages();for(unsigned slot=0;slot<8;++slot)assert(!std::memcmp(s.cpu+layout.images+slot*sizeof(image),&zero,sizeof(zero)));
  }
  auto completed=arena.Retire();completed=nullptr;assert(live.empty());
 }
 Reject([]{p::NativeDrawUpload::PlanArena(0,0,true);});
 Reject([]{p::NativeDrawUpload::PlanArena(UINT32_MAX,UINT32_MAX,true);});
 {
  std::vector<std::byte> huge(arena.DefaultBytes+4096,std::byte{0x4a});
  const auto huge_plan=p::NativeDrawUpload::PlanArena(huge.size(),0,true);
  arena.Ensure(huge_plan.bytes,budget,Allocate);assert(arena.Capacity()>=huge_plan.bytes);
  auto s=arena.Append(huge_plan.bytes,DK_MEMBLOCK_ALIGNMENT);
  {p::NativeDrawUpload upload{s.cpu,s.gpu,std::uint32_t(s.bytes),huge,{},true};
   assert(s.cpu[0]==std::byte{0x4a} && s.cpu[huge.size()-1]==std::byte{0x4a});}
  auto completed=arena.Retire();completed=nullptr;assert(live.empty());
 }
 const auto large=arena.DefaultBytes+DK_MEMBLOCK_ALIGNMENT;
 arena.Ensure(large,budget,Allocate);assert(arena.Capacity()==large);
 auto large_slice=arena.Append(large,DK_MEMBLOCK_ALIGNMENT);assert(large_slice.bytes==large && !arena.Fits(DK_MEMBLOCK_ALIGNMENT));
 Reject([&]{arena.Append(DK_MEMBLOCK_ALIGNMENT,DK_MEMBLOCK_ALIGNMENT);});
 auto block=arena.Retire();block=nullptr;assert(live.empty());
 const auto old_small=p::NativeDrawUpload::Plan(192,12,true),new_small=p::NativeDrawUpload::PlanArena(192,12,true);
 std::printf("GPU ARENA PASS layouts=%u 2048 uploads/one block; sample_bytes=%u->%u sample_per_8MiB=%llu->%llu; fallback, failure retention, large upload; MOCK SDK ONLY\n",layouts,old_small.bytes,new_small.bytes,(unsigned long long)(arena.DefaultBytes/old_small.bytes),(unsigned long long)(arena.DefaultBytes/new_small.bytes));
}
