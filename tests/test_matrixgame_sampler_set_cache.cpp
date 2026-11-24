#include "native_sampler_set_cache.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
using srhd_awa::matrixgame::platform3g::NativeSamplerSetCache;
int main(){
 NativeSamplerSetCache c;NativeSamplerSetCache::Key key{};std::uint64_t address{};
 assert(!c.Find(key,address));c.Publish(key,0x1000);assert(c.Find(key,address)&&address==0x1000);
 for(unsigned i=0;i<8;++i)for(unsigned j=0;j<8;++j){auto changed=key;changed[i][j]=1;assert(!c.Find(changed,address));}
 for(unsigned i=0;i<2048;++i)assert(c.Find(key,address)&&address==0x1000);
 auto changed=key;changed[3][2]=7;assert(!c.Find(changed,address));
 // An aborted descriptor build does not publish a new address.
 assert(c.Find(key,address)&&address==0x1000);
 c.Publish(changed,0x2000);assert(!c.Find(key,address));assert(c.Find(changed,address)&&address==0x2000);
 c.Invalidate();assert(!c.Find(changed,address));c.Publish(changed,0x3000);assert(c.Find(changed,address)&&address==0x3000);
 std::puts("sampler set exact key, publication and arena retirement PASS; CPU ONLY");
}
