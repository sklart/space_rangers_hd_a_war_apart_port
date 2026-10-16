"""Fault and concurrency probe of the actual generated cache acquisition bodies."""
from pathlib import Path
import subprocess
import sys
sys.path.insert(0, str(Path('tools').resolve()))
from e2e56_cache_overrides import apply

text = apply(Path('upstream/cpp/src/EC_Cache.cpp').read_text(encoding='utf-8'))
def body(marker):
    start = text.index(marker)
    end = text.index('\n    }', start) + len('\n    }')
    return text[start:end]
out=Path('build/e2e56/cache-probe');out.mkdir(parents=True,exist_ok=True)
code=r'''
#include <cassert>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <mutex>
#include <thread>
#include <string>
#include <string_view>
#include <vector>
#include <new>
using namespace std::string_view_literals;
namespace pas {
using WideString=std::u16string;
auto view(const WideString& s){return std::u16string_view(s);}
template<class F> struct ScopeExit {F f;ScopeExit(F a):f(a){}~ScopeExit(){f();}};
void critical_enter(std::recursive_mutex* m){m->lock();}
void critical_leave(std::recursive_mutex* m){m->unlock();}
template<class T> void free(T* p){delete p;}
template<class T,class F> T* construct_call(F){return new T;}
}
namespace srhd_awa::platform::e2e_clock {std::uint64_t Microseconds(){return 0;}}
namespace srhd_awa::platform::e2e5_failure_diagnostics {void Write(const char*){}}
namespace GlobalsV {bool HardwareRenderingEnabled=false;}
namespace GR_DX {void EvictTextureCaches(bool){}}
namespace SysUtilsImports {void Sleep(unsigned x){std::this_thread::sleep_for(std::chrono::milliseconds(x));}}
namespace WindowsSdk {void SetEvent(unsigned){}}
namespace WindowsImports {
std::atomic<int> events=0;
unsigned CreateEvent(void*,int,int,void*){++events;return 1;}
void CloseHandle(unsigned){--events;}
}
namespace EC_Buf {std::atomic<int> buffers=0; struct TBufEC{TBufEC(){++buffers;}~TBufEC(){--buffers;}};void TBufEC_Create(TBufEC*){}}
std::atomic<int> fault=0, entered=0;
void Load(int phase) {
 if(fault.load()==phase){entered=1;std::this_thread::sleep_for(std::chrono::milliseconds(60));fault=0;throw std::bad_alloc();}
}
namespace EC_Str {
int CountDelimitedPartsW(std::u16string_view,std::u16string_view){return 1;}
pas::WideString ExtractDelimitedPartW(std::u16string_view s,int,std::u16string_view){return pas::WideString(s);}
pas::WideString ExtractDelimitedRangeW(std::u16string_view s,int,int,std::u16string_view){return pas::WideString(s);}
}
namespace EC_Cache {
struct TCacheControlEC;
using TCacheDataClass=int;
struct TCacheDataEC {
 std::u16string CacheKey;unsigned LoadCompleteEvent=0;int ResidentBytes=123;
 std::vector<TCacheControlEC*> controls;
 void AppendControl(TCacheControlEC* c){controls.push_back(c);}
 void LoadFromKey(const pas::WideString&){Load(2);}
 void LoadFromConfigBuffer(EC_Buf::TBufEC*,const pas::WideString&){Load(2);}
 ~TCacheDataEC();
};
struct Root {void ReadBufferByPath(const pas::WideString&,EC_Buf::TBufEC*){Load(1);}};
struct TCacheEC {
 std::recursive_mutex lock;std::recursive_mutex* CacheLock=&lock;
 int ResidentBytes=0,ResidentByteLimit=1000;TCacheDataEC* entry=nullptr;Root root;Root* DataRoot=&root;
 TCacheDataEC* FindDataByKeyAndClass(std::u16string_view,int){return entry;}
 void AddDataToLruHead(TCacheDataEC* d){assert(!entry);entry=d;}
 void RemoveAndFreeData(TCacheDataEC* d){assert(entry==d);entry=nullptr;delete d;}
 void TouchData(TCacheDataEC*){} void TrimToBudget(int){}
 EC_Buf::TBufEC* OpenDataBuffer(const pas::WideString&);
};
struct TCacheControlEC {
 TCacheDataEC* BoundData=nullptr;int RetainCount=0;std::u16string CacheKey=u"Bm.Test";
 TCacheDataEC* CreateData(){return new TCacheDataEC;}
 TCacheDataEC* AcquireDataFromConfig(TCacheDataClass);
 TCacheDataEC* AcquireDataFromDirectKey(TCacheDataClass);
};
TCacheDataEC::~TCacheDataEC(){for(auto* c:controls)c->BoundData=nullptr;}
}
namespace GR_Main {EC_Cache::TCacheEC* GlobalCache;}
namespace EC_Cache {CONFIG
DIRECT
OPEN
}
void Unlocked(std::recursive_mutex& lock){bool ok=false;std::thread t([&]{ok=lock.try_lock();if(ok)lock.unlock();});t.join();assert(ok);}
int main(){
 for(bool direct:{false,true})for(int phase: {1,2}) {
  if(direct&&phase==1)continue;
  EC_Cache::TCacheEC cache;GR_Main::GlobalCache=&cache;
  EC_Cache::TCacheControlEC a,b;
  auto acquire=[&](auto& c){return direct?c.AcquireDataFromDirectKey(1):c.AcquireDataFromConfig(1);};
  fault=phase;entered=0;bool failed=false;
  std::thread first([&]{try{acquire(a);assert(false);}catch(const std::bad_alloc&){failed=true;}});
  while(!entered.load())std::this_thread::yield();
  std::thread second([&]{auto* d=acquire(b);assert(d&&d->LoadCompleteEvent==0);});
  first.join();second.join();Unlocked(cache.lock);
  assert(failed&&!a.BoundData&&a.RetainCount==0&&b.BoundData==cache.entry&&b.RetainCount==1);
  assert(cache.ResidentBytes==123&&EC_Buf::buffers==0&&WindowsImports::events==0);
  assert(acquire(a)==b.BoundData&&a.RetainCount==1);
  cache.RemoveAndFreeData(cache.entry);
 }
}
'''
for key, marker in [('CONFIG','    TCacheDataEC* TCacheControlEC::AcquireDataFromConfig('),
                    ('DIRECT','    TCacheDataEC* TCacheControlEC::AcquireDataFromDirectKey('),
                    ('OPEN','    EC_Buf::TBufEC* TCacheEC::OpenDataBuffer(')]:
    code=code.replace(key,body(marker))
(out/'cache.cpp').write_text(code,encoding='utf-8')
subprocess.run(['g++','-std=c++20','-O1','-g','-pthread','-fsanitize=address,undefined',str(out/'cache.cpp'),'-o',str(out/'cache')],check=True)
subprocess.run([str(out/'cache')],check=True,timeout=10)
print('cache actual bodies: read/decode bad_alloc, concurrent waiter retry, lock/buffer/event cleanup PASS')
