#ifdef NDEBUG
#error Keep benchmark assertions
#endif
#include "buffer_resources.hpp"
#include <chrono>
#include <cassert>
#include <cstdio>
#include <cstring>
namespace b=srhd_awa::matrixgame::buffers;
int main(){using Clock=std::chrono::steady_clock;b::Context context;auto* full=context.CreateVertex(4096,0x112,b::Pool::Default,true);auto* partial=context.CreateVertex(4096,0x112,b::Pool::Default,true);assert(full && partial);
 std::memset(full->Map(0,0),0,4096);full->Unmap();std::memset(partial->Map(1024,1024),0,1024);partial->Unmap();
 std::uint64_t hash=0;auto start=Clock::now();
 for(unsigned i=0;i<10000000;++i){auto* p=full->Map((i%64)*64,64,b::MapMode::NoOverwrite);std::memcpy(p,&i,4);full->Unmap();}
 auto full_us=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();assert(full->Initialized(0,4096));hash^=full->Version();start=Clock::now();
 for(unsigned i=0;i<10000000;++i){auto* p=partial->Map(1024+(i%32)*32,32);std::memcpy(p,&i,4);partial->Unmap();}
 auto partial_us=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();assert(partial->Initialized(1024,1024) && !partial->Initialized(0,4096));hash+=partial->Version();
 for(auto* v:{full,partial}){for(auto x:v->Storage())hash=(hash^std::to_integer<unsigned>(x))*1099511628211ull;v->Release();}assert(context.AllocatedBytes()==0);
 std::printf("UNMAP_PROFILE updates_each=10000000 full_us=%lld partial_us=%lld checksum=%llu CPU_BUFFER_PUBLICATION_ONLY_NOT_GPU\n",(long long)full_us,(long long)partial_us,(unsigned long long)hash);
}
