#include "native_arena_preference.hpp"
#include <cassert>
#include <cstdio>
using namespace srhd_awa::matrixgame::platform3g;
int main(){
 NativeArenaPreference p;assert(p.Preferred()==NativeArenaLargeBytes);
 p.CompletedAllocation(true);assert(p.Preferred()==NativeArenaSmallBytes);
 for(unsigned i=0;i<NativeArenaPreference::RetryAfter-1;++i){p.CompletedAllocation(false);assert(p.Preferred()==NativeArenaSmallBytes);}
 p.CompletedAllocation(false);assert(p.Preferred()==NativeArenaLargeBytes);
 p.CompletedAllocation(true);p.CompletedAllocation(false);p.CompletedAllocation(true);
 for(unsigned i=0;i<NativeArenaPreference::RetryAfter-1;++i){p.CompletedAllocation(false);assert(p.Preferred()==NativeArenaSmallBytes);}
 p.CompletedAllocation(false);assert(p.Preferred()==NativeArenaLargeBytes);
 std::puts("GPU arena fallback cooldown and retry PASS; CPU ONLY");
}
