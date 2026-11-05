#ifdef NDEBUG
#error Keep assertions
#endif
#include "texture_asset.hpp"
#include <bit>
#include <cassert>
#include <chrono>
#include <cstdio>
namespace mg=srhd_awa::matrixgame;
mg::TextureAsset Make(unsigned w,unsigned h){mg::TextureAsset t;t.clear_pixel={17,83,149,211};mg::TextureLevel l{w,h,w*4,std::vector<std::byte>(std::size_t(w)*h*4)};for(std::size_t i=0;i<l.bytes.size();++i)l.bytes[i]=std::byte((i*13+31)&255);t.levels.push_back(std::move(l));return t;}
int main(){using Clock=std::chrono::steady_clock;std::uint64_t hash=1469598103934665603ull;auto mix=[&](auto x){hash=(hash^std::uint64_t(x))*1099511628211ull;};auto base=Make(1023,767),box=Make(512,256);for(unsigned i=0;i<4;++i){auto t=base;t.PrepareMips(mg::TextureMipPolicy::OneLevel);mix(unsigned(t.levels[0].bytes[i]));}
 std::chrono::nanoseconds copy{},filter{};
 for(unsigned i=0;i<128;++i){auto t=base;auto start=Clock::now();t.PrepareMips(mg::TextureMipPolicy::OneLevel);copy+=Clock::now()-start;const auto& l=t.levels[0];assert(l.width==1024 && l.height==1024);for(std::size_t n=0;n<l.bytes.size();n+=65537)mix(unsigned(l.bytes[n]));mix(l.bytes.size());}
 for(unsigned i=0;i<64;++i){auto t=box;auto start=Clock::now();t.PrepareMips(mg::TextureMipPolicy::CompleteBox);filter+=Clock::now()-start;for(const auto& l:t.levels){mix(l.width);mix(l.height);for(auto n:{std::size_t(0),l.bytes.size()-1})mix(unsigned(l.bytes[n]));}}
 std::printf("MIP_PROFILE copies=128 source=1023x767 target=1024x1024 box_chains=64 source=512x256 copy_us=%lld box_us=%lld checksum=%llu CPU_ONLY_NOT_GPU\n",(long long)std::chrono::duration_cast<std::chrono::microseconds>(copy).count(),(long long)std::chrono::duration_cast<std::chrono::microseconds>(filter).count(),(unsigned long long)hash);
}
