#include "native_weak_resource_cache.hpp"
#include <cassert>
#include <cstdio>
#include <memory>
#include <stdexcept>
using srhd_awa::matrixgame::platform3g::NativeWeakResourceCache;
struct Image{int descriptor;};
int main(){
 NativeWeakResourceCache<Image,int,8> cache;unsigned conversions=0;bool reused=false;
 auto convert=[&](const auto& p){++conversions;return p->descriptor;};
 auto a=std::make_shared<Image>(Image{10});std::weak_ptr<Image> weak=a;
 assert(cache.Get(0,a,convert,reused)==10 && !reused && conversions==1);
 for(unsigned i=0;i<1000;++i)assert(cache.Get(0,a,convert,reused)==10 && reused && conversions==1);
 assert(cache.Get(1,a,convert,reused)==10 && !reused && conversions==2);
 auto b=std::make_shared<Image>(Image{20});
 try{cache.Get(0,b,[](const auto&)->int{throw std::runtime_error("convert");},reused);assert(false);}catch(const std::runtime_error&){}
 assert(cache.Get(0,a,convert,reused)==10 && reused && conversions==2);
 assert(cache.Get(0,b,convert,reused)==20 && !reused && conversions==3);
 a.reset();assert(weak.expired()); // slot 1 must not hold the resource alive.
 auto c=std::make_shared<Image>(Image{30});assert(cache.Get(1,c,convert,reused)==30 && !reused);
 struct Pair{Image first{40},second{50};};auto pair=std::make_shared<Pair>();
 auto left=std::shared_ptr<Image>(pair,&pair->first),right=std::shared_ptr<Image>(pair,&pair->second);
 assert(cache.Get(2,left,convert,reused)==40 && !reused);
 assert(cache.Get(2,right,convert,reused)==50 && !reused);
 assert(cache.Get(2,right,convert,reused)==50 && reused);
 try{cache.Get(8,c,convert,reused);assert(false);}catch(const std::invalid_argument&){}
 try{cache.Get(0,std::shared_ptr<Image>{},convert,reused);assert(false);}catch(const std::invalid_argument&){}
 std::puts("weak descriptor cache identity, expiry, failure, slot isolation PASS; CPU ONLY");
}
