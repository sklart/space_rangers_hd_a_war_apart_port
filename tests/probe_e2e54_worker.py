"""Inject an exception into the actual generated ProcessRequests loop."""
from pathlib import Path
import sys,subprocess
root=Path.cwd();sys.path.insert(0,str(root/'tools'))
from e2e_source_overrides import generated_source
out=root/'build/e2e54';out.mkdir(parents=True,exist_ok=True)
p=generated_source(root/'upstream/cpp/src/EC_Thread.cpp',out/'EC_Thread.cpp',audio_on=True,e2e5_profile=True)
s=p.read_text();s=s[s.index('    void TThreadEC::ProcessRequests() {'):s.index('    void TThreadEC_Execute(')]
body=s[s.index('{'):]
code=r'''
#include "e2e_threads.hpp"
#include "e2e_events.hpp"
#include <array>
#include <cassert>
#include <exception>
#include <mutex>
#include <new>
#include <string>
namespace events=srhd_awa::platform::e2e_events;
namespace threads=srhd_awa::platform::e2e_threads;
std::string operator""_a(const char* s,std::size_t n) {return std::string(s,n);}
namespace pas {
template<class F> struct ScopeExit {F f; ScopeExit(F v):f(v){} ~ScopeExit() noexcept {f();}};
template<class T,int A,int B> using Array=std::array<T,B-A+1>;
void critical_enter(std::recursive_mutex* m) {m->lock();}
void critical_leave(std::recursive_mutex* m) {m->unlock();}
void* caught_object() {return nullptr;}
struct Exception {std::string message;};
template<class T> T class_cast_if(void*) {return nullptr;}
}
namespace WindowsImports {using THandle=std::uint32_t;}
namespace WindowsSdk {
using PWOHandleArray=const std::uint32_t*;
constexpr auto WAIT_OBJECT_0=0u, INFINITE=0xffffffffu;
auto WaitForMultipleObjects(unsigned n,const std::uint32_t* h,int all,unsigned timeout) {return events::WaitMany(h,n,all,timeout);}
void ResetEvent(unsigned h) {assert(events::Reset(h));}
void SetEvent(unsigned h) {assert(events::Set(h));}
}
namespace GR_Main {void AppendLogLineThreadSafe(const std::string&) {}}
struct Mock {
 std::recursive_mutex mutex;std::recursive_mutex* Lock=&mutex;
 unsigned ShutdownEvent=events::Create(true,false),StartEvent=events::Create(false,true),
 StopEvent=events::Create(true,false),IdleEvent=events::Create(true,false),RunningEvent=events::Create(true,true);
 bool StopRequested=false;
 bool IsRunning() {return events::WaitOne(IdleEvent,0)==events::kWaitTimeout;}
 void virtual_TThreadEC_Execute() {throw std::bad_alloc();}
 void ProcessRequests() BODY
};
std::int32_t Entry(void* opaque) {static_cast<Mock*>(opaque)->ProcessRequests();return 0;}
int main() {
 Mock m;unsigned id=0;auto worker=threads::Create(Entry,&m,true,&id);assert(worker);
 assert(threads::Resume(worker)==1);
 assert(events::WaitOne(m.IdleEvent,2000)==events::kWaitObject0);
 assert(events::WaitOne(m.RunningEvent,0)==events::kWaitTimeout);
 assert(threads::Wait(worker,2000)==events::kWaitFailed);
 assert(!threads::IsUsable(worker));assert(threads::Close(worker));
 for(auto h:{m.ShutdownEvent,m.StartEvent,m.StopEvent,m.IdleEvent,m.RunningEvent}) assert(events::Close(h));
}
'''.replace('BODY',body)
(out/'worker-failure.cpp').write_text(code)
subprocess.run(['g++','-std=c++20','-O1','-g','-pthread','-fsanitize=address,undefined','-Iport/switch/platform',str(out/'worker-failure.cpp'),'port/switch/platform/e2e_threads.cpp','port/switch/platform/e2e_events.cpp','-o',str(out/'worker-failure')],check=True)
subprocess.run([str(out/'worker-failure')],check=True)
print('actual ProcessRequests + real event/thread backends: bad_alloc releases idle/running events and marks dead worker unusable PASS')
