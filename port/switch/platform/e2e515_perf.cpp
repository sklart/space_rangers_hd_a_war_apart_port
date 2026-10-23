#include "e2e515_perf.hpp"
#include <functional>
#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif
#include "e2e_clock.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <thread>
#include <ctime>

namespace srhd_awa::platform::perf515 {
namespace {
struct ThreadSlot {
 std::atomic<std::uint64_t> version{},token{},since{},progress{};
 std::atomic<const char*> phase{"idle"};
 std::atomic<unsigned> stage{};
 std::atomic<bool> native{};
};
std::array<ThreadSlot,static_cast<unsigned>(ThreadRole::Count)> thread_slots;
constexpr const char* thread_names[]{"main","turn","saver","writer"};
}
ThreadSnapshot InspectThread(ThreadRole role)noexcept {
 auto& slot=thread_slots[static_cast<unsigned>(role)];
 ThreadSnapshot value;
 for(int retry=0;retry<2;++retry){
  const auto before=slot.version.load(std::memory_order_acquire);
  if(before&1)continue;
  value.phase=slot.phase.load();value.thread_token=slot.token.load();
  value.since_us=slot.since.load();value.progress=slot.progress.load();
  value.stage=slot.stage.load();value.native_thread_id=slot.native.load();
  value.consistent=before==slot.version.load(std::memory_order_acquire);
  if(value.consistent)return value;
 }
 value.consistent=false;return value;
}
void MarkThread(ThreadRole role,const char* phase,unsigned stage,std::uint64_t now)noexcept {
 if(!Mode())return;
 struct Identity {std::uint64_t value;bool native;};
 static thread_local const Identity identity=[]()noexcept{
#if defined(__SWITCH__)
  u64 id{};if(R_SUCCEEDED(svcGetThreadId(&id,CUR_THREAD_HANDLE)))return Identity{id,true};
#elif defined(__linux__)
  return Identity{static_cast<std::uint64_t>(syscall(SYS_gettid)),true};
#endif
  return Identity{static_cast<std::uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())),false};
 }();
 if(!now)now=e2e_clock::Microseconds();
 auto& slot=thread_slots[static_cast<unsigned>(role)];
 slot.version.fetch_add(1,std::memory_order_acq_rel);
 slot.token.store(identity.value);slot.native.store(identity.native);
 slot.phase.store(phase);slot.stage.store(stage);slot.since.store(now);slot.progress.fetch_add(1);
 slot.version.fetch_add(1,std::memory_order_release);
}
ThreadScope::ThreadScope(ThreadRole role,const char* phase,unsigned stage)noexcept:role_(role),enabled_(Mode()!=0){
 if(enabled_){previous_=InspectThread(role);MarkThread(role,phase,stage);}
}
ThreadScope::~ThreadScope(){if(enabled_)MarkThread(role_,previous_.consistent?previous_.phase:"returned",previous_.stage);}
namespace {
constexpr std::size_t Capacity=8192, PerfLimit=16*1024*1024, LogLimit=8*1024*1024;
struct Entry {bool log{};char text[640]{};};
std::array<Entry,Capacity> queue;
std::size_t head{},tail{},count{};
std::mutex mutex;
std::atomic<int> mode{};
std::atomic<bool> scene_enabled{};
constexpr int ImageWidth=640,ImageHeight=360,ImageLimit=48;
struct ImageEntry {
 std::atomic<int> state{}; // 0 free, 1 filling, 2 ready, 3 writer owns
 unsigned sequence{};
 char metadata[640]{};
 std::array<std::uint16_t,ImageWidth*ImageHeight> pixels{};
};
std::array<ImageEntry,4> images;
std::atomic<unsigned> image_sequence{},image_dropped{};
std::uint64_t image_run{};
std::atomic<bool> running{};
std::atomic<std::uint64_t> dropped{},truncated{},last_frame{},max_gap{},ticks{};
std::atomic<int> screen{-1};
std::atomic<const char*> active{"idle"},completed{"none"};
std::atomic<std::uint64_t> active_since{};
std::thread writer;
std::uint64_t frame_ticks{};
std::uint64_t Now(){return e2e_clock::Microseconds();}
void Push(bool log,const char* text) {
  if(!running.load(std::memory_order_relaxed))return;
  std::unique_lock lock(mutex,std::try_to_lock);
  if(!lock.owns_lock()||count==Capacity){++dropped;return;}
  auto& entry=queue[tail];entry.log=log;
  const auto n=std::snprintf(entry.text,sizeof(entry.text),"%s",text);
  if(n<0||static_cast<std::size_t>(n)>=sizeof(entry.text))++truncated;
  tail=(tail+1)%Capacity;++count;
}
struct Sink {
 FILE* file{};std::size_t bytes{},limit{};bool failed{},capped{};
 Sink(const std::filesystem::path& path,std::size_t cap):limit(cap) {
   std::error_code ec;
   auto previous=path;previous+=".1";
   if(std::filesystem::exists(path,ec)) {
     std::filesystem::remove(previous,ec);ec.clear();
     std::filesystem::rename(path,previous,ec);
     if(ec){failed=true;return;}
   }
   file=std::fopen(path.generic_string().c_str(),"w");failed=!file;
 }
 void Write(const char* text) {
   if(!file||failed||capped)return;
   const auto n=std::strlen(text);
   if(bytes+n+1>limit-256) {
     const char stop[]="# file_cap_reached; remaining samples unavailable\n";
     std::fwrite(stop,1,sizeof(stop)-1,file);capped=true;return;
   }
   if(std::fwrite(text,1,n,file)!=n||std::fputc('\n',file)==EOF)failed=true;
   bytes+=n+1;
 }
 void Flush(){if(file&&std::fflush(file)!=0)failed=true;}
 ~Sink(){if(file)std::fclose(file);}
};
void WriteImage(const std::filesystem::path& root,Sink& log) {
 for(auto& image:images) {
  int expected=2;
  if(!image.state.compare_exchange_strong(expected,3,std::memory_order_acquire))continue;
  char name[64]{};
  std::snprintf(name,sizeof(name),"scene-%llu-%03u.ppm",static_cast<unsigned long long>(image_run),image.sequence);
  const auto path=root/"logs"/name;
  // Unique run sequence plus exclusive creation: never overwrite a prior sample.
  FILE* f=std::fopen(path.generic_string().c_str(),"wbx");
  bool ok=f && std::fprintf(f,"P6\n%d %d\n255\n",ImageWidth,ImageHeight)>0;
  std::array<unsigned char,ImageWidth*3> row{};
  for(int y=0;y<ImageHeight && ok;++y) {
   for(int x=0;x<ImageWidth;++x) {
    const auto c=image.pixels[y*ImageWidth+x];
    row[x*3]=((c>>11)&31)*255/31;row[x*3+1]=((c>>5)&63)*255/63;row[x*3+2]=(c&31)*255/31;
   }
   ok=std::fwrite(row.data(),1,row.size(),f)==row.size();
  }
  if(f && std::fclose(f))ok=false;
  char line[768]{};
  std::snprintf(line,sizeof(line),"%s file=%s image_ok=%d image_dropped=%u",image.metadata,name,int(ok),image_dropped.load());
  log.Write(line);
  image.state.store(0,std::memory_order_release);
 }
}
void Writer(std::filesystem::path root) {
  std::error_code ec;std::filesystem::create_directories(root/"logs",ec);
  Sink perf(root/"logs/perf515.csv",PerfLimit),log(root/"logs/perf515-stage.log",LogLimit);
  perf.Write("stage,mono_us,epoch_ms,wall_us,cpu_us,screen,a,b");
  std::array<Entry,16> batch; // Keep below the libnx worker stack budget.
  auto next=Now();
  while(true) {
    MarkThread(ThreadRole::Writer,"writer-io");
    std::size_t size{};bool done{};
    {std::lock_guard lock(mutex);
      while(count&&size<batch.size()) {batch[size++]=queue[head];head=(head+1)%Capacity;--count;}
      done=!running.load()&&count==0;
    }
    for(std::size_t i=0;i<size;++i)(batch[i].log?log:perf).Write(batch[i].text);
    WriteImage(root,log);
    const auto now=Now();
    if((now>=next||done) && Mode()) {
      timespec real{};const int time_rc=clock_gettime(CLOCK_REALTIME,&real);
      const auto after=Now();char line[640]{};
      std::snprintf(line,sizeof(line),"anchor,%llu,%lld,%llu,-1,%d,%d,0",
        static_cast<unsigned long long>(now),time_rc? -1LL:static_cast<long long>(real.tv_sec)*1000+real.tv_nsec/1000000,
        static_cast<unsigned long long>(after-now),screen.load(),time_rc);perf.Write(line);
      const auto last=last_frame.load();const auto since=active_since.load();
      std::snprintf(line,sizeof(line),"heartbeat,%llu,-1,%llu,-1,%d,%llu,%llu",
        static_cast<unsigned long long>(now),static_cast<unsigned long long>(last&&now>=last?now-last:0),screen.load(),
        static_cast<unsigned long long>(max_gap.load()),static_cast<unsigned long long>(ticks.load()));perf.Write(line);
      std::snprintf(line,sizeof(line),"mono_us=%llu active=%s active_age_us=%llu last_completed=%s dropped=%llu truncated=%llu perf_failed=%d log_failed=%d capped=%d/%d",
        static_cast<unsigned long long>(now),active.load(),static_cast<unsigned long long>(since&&now>=since?now-since:0),completed.load(),
        static_cast<unsigned long long>(dropped.load()),static_cast<unsigned long long>(truncated.load()),perf.failed,log.failed,perf.capped,log.capped);log.Write(line);
      for(unsigned role=0;role<static_cast<unsigned>(ThreadRole::Count);++role){
        const auto state=InspectThread(static_cast<ThreadRole>(role));
        if(!state.thread_token && state.consistent)continue;
        std::snprintf(line,sizeof(line),"mono_us=%llu watchdog role=%s thread_token=%llu native_id=%d phase=%s stage=%u age_us=%llu progress=%llu consistent=%d",
          static_cast<unsigned long long>(now),thread_names[role],static_cast<unsigned long long>(state.thread_token),int(state.native_thread_id),
          state.phase,state.stage,static_cast<unsigned long long>(now>=state.since_us?now-state.since_us:0),
          static_cast<unsigned long long>(state.progress),int(state.consistent));log.Write(line);
      }
#if defined(__SWITCH__)
      u64 used{},total{},program{};
      const auto ur=svcGetInfo(&used,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0);
      const auto tr=svcGetInfo(&total,InfoType_TotalMemorySize,CUR_PROCESS_HANDLE,0);
      const auto pr=svcGetInfo(&program,InfoType_ProgramId,CUR_PROCESS_HANDLE,0);
      std::snprintf(line,sizeof(line),"memory,%llu,-1,0,-1,%d,%lld,%lld",static_cast<unsigned long long>(Now()),screen.load(),
        R_FAILED(ur)?-1LL:static_cast<long long>(used),R_FAILED(tr)?-1LL:static_cast<long long>(total));perf.Write(line);
      std::snprintf(line,sizeof(line),"mono_us=%llu applet_type=%d operation_mode=%d program_id=%016llx info_rc=%x/%x/%x",
        static_cast<unsigned long long>(Now()),int(appletGetAppletType()),int(appletGetOperationMode()),static_cast<unsigned long long>(program),ur,tr,pr);log.Write(line);
#endif
      const auto io_start=Now();perf.Flush();log.Flush();
      std::snprintf(line,sizeof(line),"writer_io,%llu,-1,%llu,-1,%d,%llu,%d",static_cast<unsigned long long>(Now()),
        static_cast<unsigned long long>(Now()-io_start),screen.load(),static_cast<unsigned long long>(dropped.load()),int(perf.failed||log.failed));perf.Write(line);
      next=now+1000000;
    }
    if(!Mode()&&(now>=next||done)){log.Flush();next=now+1000000;}
    if(done){log.Write("[WATCHDOG] writer queue drained; leaving writer loop; stream close follows");break;}
    MarkThread(ThreadRole::Writer,"writer-idle");
    if(size==0)e2e_clock::SleepMilliseconds(20);
  }
}
} // namespace
int Mode(){return mode.load(std::memory_order_relaxed);}
bool SceneEnabled(){return scene_enabled.load(std::memory_order_relaxed) && Mode() && running.load(std::memory_order_relaxed);}
void SceneImage(const char* event,int id,std::uint64_t generation,
                const void* owner,const void* buffer,const void* pixels,
                int pitch,int width,int height,int cursor_x,int cursor_y,
                std::int64_t turn,std::int64_t film,std::int64_t order,std::uint64_t frame) {
 if(!SceneEnabled() || !pixels || width<1 || height<1 || pitch<width*2 || width>16384 || height>16384)return;
 if(image_sequence.load()>=ImageLimit)return;
 ImageEntry* image=nullptr;
 for(auto& candidate:images) {
  int expected=0;
  if(candidate.state.compare_exchange_strong(expected,1,std::memory_order_acquire)){image=&candidate;break;}
 }
 if(!image){++image_dropped;return;}
 image->sequence=++image_sequence;
 const auto began=Now();
 std::uint64_t center=1469598103934665603ull,panel=center,cursor=center;
 auto hash=[](std::uint64_t& h,std::uint16_t c){h^=c&255;h*=1099511628211ull;h^=c>>8;h*=1099511628211ull;};
 for(int y=0;y<ImageHeight;++y) {
  const int sy=y*height/ImageHeight;
  const auto* row=reinterpret_cast<const std::uint16_t*>(static_cast<const unsigned char*>(pixels)+sy*pitch);
  for(int x=0;x<ImageWidth;++x) {
   const int sx=x*width/ImageWidth;const auto c=row[sx];
   image->pixels[y*ImageWidth+x]=c;
   if(x>=ImageWidth/4 && x<ImageWidth*3/4 && y>=ImageHeight/6 && y<ImageHeight*5/6)hash(center,c);
   if(y>=ImageHeight*9/10)hash(panel,c);
   if(sx>=cursor_x-24 && sx<cursor_x+24 && sy>=cursor_y-24 && sy<cursor_y+24)hash(cursor,c);
  }
 }
 std::snprintf(image->metadata,sizeof(image->metadata),
  "mono_us=%llu [SCENE] event=%s screen=%d generation=%llu frame=%llu owner=%p buffer=%p width=%d height=%d pitch=%d format=RGB565 cursor_x=%d cursor_y=%d turn=%lld film=%lld order=%lld center_hash=%016llx panel_hash=%016llx cursor_hash=%016llx copy_us=%llu",
  static_cast<unsigned long long>(began),event,id,static_cast<unsigned long long>(generation),static_cast<unsigned long long>(frame),owner,buffer,width,height,pitch,cursor_x,cursor_y,
  static_cast<long long>(turn),static_cast<long long>(film),static_cast<long long>(order),static_cast<unsigned long long>(center),static_cast<unsigned long long>(panel),static_cast<unsigned long long>(cursor),static_cast<unsigned long long>(Now()-began));
 image->state.store(2,std::memory_order_release);
}
void Initialize(const char* root) {
#if defined(E2E515_PERF_DIAGNOSTICS) && E2E515_PERF_DIAGNOSTICS
  if(running.load())return;
  for(auto& slot:thread_slots){
    slot.version.store(0);slot.token.store(0);slot.since.store(0);slot.progress.store(0);
    slot.phase.store("idle");slot.stage.store(0);slot.native.store(false);
  }
  image_run=Now();image_sequence.store(0);image_dropped.store(0);
  int value=1;char setting[32]{};
  const auto config=std::filesystem::path(root)/"perf-mode.txt";
  if(FILE* f=std::fopen(config.generic_string().c_str(),"r")) {
    if(std::fgets(setting,sizeof(setting),f)) {
      if(std::strncmp(setting,"off",3)==0)value=0;
      else if(std::strncmp(setting,"detail",6)==0)value=2;
    }
    std::fclose(f);
  }
  scene_enabled.store(false);
  const auto scene_config=std::filesystem::path(root)/"scene-mode.txt";
  if(FILE* f=std::fopen(scene_config.generic_string().c_str(),"r")) {
    if(std::fgets(setting,sizeof(setting),f))scene_enabled.store(std::strncmp(setting,"on",2)==0);
    std::fclose(f);
  }
  mode.store(value);running.store(true);
  try{writer=std::thread([path=std::filesystem::path(root)]{
    try{Writer(path);}catch(...){running.store(false);mode.store(0);}
  });}
  catch(...){running.store(false);mode.store(0);std::fprintf(stderr,"perf515 writer unavailable; diagnostics disabled\n");return;}
  Record("session",0,-1,value,0);
#else
  (void)root;
#endif
}
void Shutdown(){
 ThreadScope stage(ThreadRole::Main,"perf-shutdown-join");
 AsyncLog("[WATCHDOG] ","shutdown drain/join begin");
 running.store(false);if(writer.joinable())writer.join();
}
void AsyncLog(const char* prefix,const char* message) {
  char line[640]{};const auto n=std::snprintf(line,sizeof(line),"mono_us=%llu %s%s",static_cast<unsigned long long>(Now()),prefix,message);
  if(n<0||static_cast<std::size_t>(n)>=sizeof(line))++truncated;
  Push(true,line);
}
void Record(const char* stage,std::uint64_t wall,std::int64_t cpu,std::int64_t a,std::int64_t b) {
  if(!Mode()&&std::strcmp(stage,"session"))return;
  if(std::strcmp(stage,"logical_tick")==0)ticks.fetch_add(1);
  char line[256]{};
  std::snprintf(line,sizeof(line),"%s,%llu,-1,%llu,%lld,%d,%lld,%lld",stage,
    static_cast<unsigned long long>(Now()),static_cast<unsigned long long>(wall),static_cast<long long>(cpu),screen.load(),static_cast<long long>(a),static_cast<long long>(b));
  Push(false,line);
}
std::int64_t CpuMicros() {
#if defined(__SWITCH__)
  u64 ticks{};
  if(R_FAILED(svcGetInfo(&ticks,InfoType_ThreadTickCount,CUR_THREAD_HANDLE,TickCountInfo_Total)))return -1;
  return armTicksToNs(ticks)/1000;
#else
  timespec t{};if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t))return -1;
  return static_cast<std::int64_t>(t.tv_sec)*1000000+t.tv_nsec/1000;
#endif
}
Scope::Scope(const char* stage,bool detail) {
  if(!Mode()||(detail&&Mode()<2))return;
  stage_=stage;parent_=active.load();parent_start_=active_since.load();
  start_=Now();cpu_=CpuMicros();active.store(stage);active_since.store(start_);
}
Scope::~Scope(){if(stage_) {
  const auto end=Now();const auto cpu=CpuMicros();
  Record(stage_,end-start_,cpu>=0&&cpu_>=0&&cpu>=cpu_?cpu-cpu_:-1);
  completed.store(stage_);active.store(parent_);active_since.store(parent_start_);
}}
void Frame(int value) {
  if(!Mode())return;screen.store(value);
  const auto now=Now(),previous=last_frame.exchange(now),n=ticks.load();
  const auto gap=previous&&now>=previous?now-previous:0;
  if(gap>max_gap.load())max_gap.store(gap);
  Record("frame",gap,-1,n-frame_ticks,max_gap.load());frame_ticks=n;
}
void TickBatch(std::uint64_t pending,std::uint32_t delivered){Record("backlog",0,-1,pending,delivered);}
} // namespace srhd_awa::platform::perf515
