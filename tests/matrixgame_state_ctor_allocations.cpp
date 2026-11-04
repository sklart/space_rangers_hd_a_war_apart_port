#include "native_device_state.hpp"
#include "native_state_layout.hpp"
#include "matrixgame_state_ctor_probe.hpp"
#include <cstdlib>
#include <new>
#include <cassert>
namespace {thread_local bool active=false;thread_local std::int64_t allowance=-1;thread_local std::uint64_t attempts=0,live=0;
void* Allocate(std::size_t n){if(active){++attempts;if(allowance==0)throw std::bad_alloc();if(allowance>0)--allowance;}auto* p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();if(active)++live;return p;}
void Release(void* p)noexcept{if(p){if(active){assert(live);--live;}std::free(p);}}
}
void* operator new(std::size_t n){return Allocate(n);}void* operator new[](std::size_t n){return Allocate(n);}
void operator delete(void* p)noexcept{Release(p);}void operator delete[](void* p)noexcept{Release(p);}
void operator delete(void* p,std::size_t)noexcept{Release(p);}void operator delete[](void* p,std::size_t)noexcept{Release(p);}
CtorAllocationResult ConstructorAllocationProbe(std::int64_t allowed){
 assert(!active);allowance=allowed;attempts=live=0;bool failed=false;std::uint64_t hash=1469598103934665603ull;active=true;
 try{srhd_awa::matrixgame::platform3g::NativeRenderState state;assert(state.registers.size()==srhd_awa::matrixgame::platform3g::NativeRenderWords.size());
  for(const auto& word:srhd_awa::matrixgame::platform3g::NativeRenderWords){assert(state.registers.at(word.id)==word.initial);hash=(hash^word.id^state.registers.at(word.id))*1099511628211ull;}
 }catch(const std::bad_alloc&){failed=true;}catch(...){active=false;throw;}
 active=false;assert(live==0);return {attempts,live,hash,failed};
}
