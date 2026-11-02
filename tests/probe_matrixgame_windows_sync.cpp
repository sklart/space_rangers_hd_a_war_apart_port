// Actual SDK ABI check plus native recursive locking and contended ownership.
#include "sync.hpp"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <thread>
extern "C" std::size_t WindowsCriticalSize();
extern "C" std::size_t WindowsCriticalAlignment();
int main(){
 assert(sizeof(pas::CriticalSection::section)==WindowsCriticalSize());
 assert(alignof(pas::CriticalSection)>=WindowsCriticalAlignment());
 for(int cycle=0;cycle<4;++cycle){
  auto* lock=pas::make_critical_section<pas::CriticalSection>();
  unsigned count{};
  auto work=[&]{for(int i=0;i<10000;++i){pas::critical_enter(lock);pas::critical_enter(lock);++count;pas::critical_leave(lock);pas::critical_leave(lock);}};
  std::thread worker(work);work();worker.join();assert(count==20000);pas::free(lock);
 }
 std::printf("WINDOWS_SYNC sdk_size=%zu sdk_align=%zu cycles=4 recursive=PASS contended=PASS cleanup=PASS\n",WindowsCriticalSize(),WindowsCriticalAlignment());
}
