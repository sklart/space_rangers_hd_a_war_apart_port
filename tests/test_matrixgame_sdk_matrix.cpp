// Execute the actual relocation-free SDK Multiply machine code under QEMU.
#include <array>
#include <bit>
#include <cassert>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#ifdef NDEBUG
#error Keep SDK binary assertions
#endif
using Matrix=std::array<float,16>;
extern "C" Matrix SDKMultiply(const Matrix&,const Matrix&);
int main(){
 unsigned cases=0;std::uint32_t rng=0x82941263;
 auto identity=[](){return Matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};};
 assert(std::fesetround(FE_TONEAREST)==0);const auto id=identity();const auto abi=SDKMultiply(id,id);assert(!std::memcmp(abi.data(),id.data(),sizeof(id)));
 auto check=[&](const Matrix& a,const Matrix& b){const auto before_a=a,before_b=b;
  std::feclearexcept(FE_ALL_EXCEPT);const auto actual=SDKMultiply(a,b);const int flags=std::fetestexcept(FE_ALL_EXCEPT);std::feclearexcept(FE_ALL_EXCEPT);
  assert(!std::memcmp(a.data(),before_a.data(),sizeof(a))&&!std::memcmp(b.data(),before_b.data(),sizeof(b)));
  std::printf("SDK_CASE id=%u round=%d flags=%d",cases++,std::fegetround(),flags);
  for(float v:actual)std::printf(" %08x",std::bit_cast<std::uint32_t>(v));std::puts("");
 };
 for(int mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){assert(std::fesetround(mode)==0);
  for(unsigned n=0;n<512;++n){Matrix a{},b{};for(auto* m:{&a,&b})for(float& v:*m){rng=rng*1664525u+1013904223u;v=std::bit_cast<float>((rng&0x83ffffffu)|0x3c000000u);}check(a,b);check(a,a);}
  for(unsigned bits:{0u,0x80000000u,1u,0x80000001u,0x00800000u,0x80800000u,0x3f800000u,0xbf800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc12345u,0x7f812345u})for(unsigned side=0;side<2;++side)for(unsigned field=0;field<16;++field){auto a=identity(),b=identity();(side?b:a)[field]=std::bit_cast<float>(bits);check(a,b);check(a,a);}
  for(unsigned pattern=0;pattern<16;++pattern){Matrix a{},b{};for(unsigned i=0;i<16;++i){a[i]=std::bit_cast<float>(i==pattern?0x7f812345u:0x3f800000u);b[i]=std::bit_cast<float>(i==pattern?0x7fc54321u:0x80000000u);}check(a,b);}
 }
 assert(cases==7744);std::puts("SDK_MATRIX_BINARY cases=7744 full64_and_FPflags SOFTWARE_ONLY_NOT_SWITCH");
}
