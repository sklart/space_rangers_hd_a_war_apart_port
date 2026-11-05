#ifdef NDEBUG
#error Keep assertions
#endif
#include "texture_asset.hpp"
#include <algorithm>
#include <bit>
#include <cassert>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
namespace mg=srhd_awa::matrixgame;
std::uint64_t hash=1469598103934665603ull,cases{},rejected{},oracle_pixels{};
void Mix(std::uint64_t n){hash=(hash^n)*1099511628211ull;}
void Image(const mg::TextureAsset& t){Mix(unsigned(t.format));Mix(t.png_gamma);for(auto c:t.clear_pixel)Mix(c);Mix(t.levels.size());for(const auto& l:t.levels){Mix(l.width);Mix(l.height);Mix(l.pitch);Mix(l.bytes.size());for(auto b:l.bytes)Mix(unsigned(b));}}
mg::TextureAsset Make(unsigned w,unsigned h,unsigned pad,unsigned gamma=0,mg::TextureFormat format=mg::TextureFormat::RGBA8){mg::TextureAsset t;t.format=format;t.png_gamma=gamma;t.clear_pixel={17,83,149,211};const auto type=unsigned(format),block=type?(type==1?8u:16u):4u,extent=type?4u:1u;const unsigned row=((w+extent-1)/extent)*block,rows=(h+extent-1)/extent,pitch=row+pad;mg::TextureLevel l{w,h,pitch,std::vector<std::byte>(std::size_t(rows-1)*pitch+row+7)};for(std::size_t i=0;i<l.bytes.size();++i)l.bytes[i]=std::byte((i*13+31)&255);t.levels.push_back(std::move(l));return t;}
template<class F>void Observe(mg::TextureAsset& t,F action){const auto before=t;const auto* pointer=t.levels.empty()?nullptr:t.levels[0].bytes.data();++cases;Mix(cases);std::feclearexcept(FE_ALL_EXCEPT);try{action();Mix(std::fetestexcept(FE_ALL_EXCEPT));Mix(1);Image(t);}catch(const std::exception& e){Mix(std::fetestexcept(FE_ALL_EXCEPT));++rejected;Mix(0);Mix(dynamic_cast<const std::invalid_argument*>(&e)?1:dynamic_cast<const std::length_error*>(&e)?2:3);for(auto c=e.what();*c;++c)Mix(static_cast<unsigned char>(*c));assert(t.format==before.format && t.png_gamma==before.png_gamma && t.clear_pixel==before.clear_pixel && t.levels.size()==before.levels.size());for(unsigned i=0;i<t.levels.size();++i){assert(t.levels[i].width==before.levels[i].width && t.levels[i].height==before.levels[i].height && t.levels[i].pitch==before.levels[i].pitch && t.levels[i].bytes==before.levels[i].bytes);}assert(t.levels.empty() || pointer==t.levels[0].bytes.data());Image(t);}std::feclearexcept(FE_ALL_EXCEPT);}
int main(int argc,char** argv){
 if(argc==2 && !std::strcmp(argv[1],"--unaligned-bc")){
  unsigned checked=0;const int previous=std::fegetround();
  for(int mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){assert(std::fesetround(mode)==0);
   for(auto format:{mg::TextureFormat::BC1,mg::TextureFormat::BC2,mg::TextureFormat::BC3})for(unsigned w:{1u,4u,5u,9u})for(unsigned h:{1u,3u,5u})for(auto policy:{mg::TextureMipPolicy::OneLevel,mg::TextureMipPolicy::CompleteBox})for(unsigned padding:{1u,3u,7u}){
    auto odd=Make(w,h,padding,0,format),aligned=Make(w,h,0,0,format);const auto block=format==mg::TextureFormat::BC1?8u:16u,row=((w+3)/4)*block,rows=(h+3)/4;
    for(unsigned y=0;y<rows;++y)std::memcpy(aligned.levels[0].bytes.data()+std::size_t(y)*row,odd.levels[0].bytes.data()+std::size_t(y)*odd.levels[0].pitch,row);
    std::feclearexcept(FE_ALL_EXCEPT);aligned.PrepareMips(policy);const int flags=std::fetestexcept(FE_ALL_EXCEPT);std::feclearexcept(FE_ALL_EXCEPT);odd.PrepareMips(policy);assert(std::fetestexcept(FE_ALL_EXCEPT)==flags);std::feclearexcept(FE_ALL_EXCEPT);assert(odd.levels.size()==aligned.levels.size());
    for(unsigned i=0;i<odd.levels.size();++i){const auto& a=aligned.levels[i];const auto& b=odd.levels[i];assert(a.width==b.width && a.height==b.height);const auto rb=((a.width+3)/4)*block,nr=(a.height+3)/4;for(unsigned y=0;y<nr;++y){assert(!std::memcmp(a.bytes.data()+std::size_t(y)*a.pitch,b.bytes.data()+std::size_t(y)*b.pitch,rb));for(unsigned x=0;x<rb;++x)Mix(unsigned(b.bytes[std::size_t(y)*b.pitch+x]));}}
    Mix(flags);++checked;
   }
  }
  assert(std::fesetround(previous)==0 && checked==864);std::printf("BC_ALIGNMENT cases=%u checksum=%llu all3_codecs odd_pitch_1_3_7 same_active_bytes_and_fenv_as_aligned fenv4 CPU_ONLY_NOT_GPU\n",checked,(unsigned long long)hash);return 0;
 }
 const int previous=std::fegetround();
 for(int mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){assert(std::fesetround(mode)==0);
  for(unsigned w:{1u,2u,3u,7u,8u,9u,15u,16u,17u,31u,65u,257u})for(unsigned h:{1u,2u,7u,17u})for(unsigned pad:{0u,13u})for(unsigned gamma:{0u,45000u})for(auto policy:{mg::TextureMipPolicy::OneLevel,mg::TextureMipPolicy::CompleteBox}){
   auto t=Make(w,h,pad,gamma),before=t;Observe(t,[&]{t.PrepareMips(policy);});assert(t.png_gamma==0 && !t.levels.empty());const auto& l=t.levels.front();assert(l.width==std::bit_ceil(w) && l.height==std::bit_ceil(h));
   const bool copied=!gamma && l.width==w && l.height==h;
   if(copied){assert(l.bytes==before.levels[0].bytes && l.pitch==before.levels[0].pitch);}
   for(unsigned y=0;y<l.height;++y)for(unsigned x=0;x<l.width;++x)for(unsigned c=0;c<4;++c){unsigned expected=x<w && y<h?unsigned(before.levels[0].bytes[std::size_t(y)*before.levels[0].pitch+x*4+c]):before.clear_pixel[c];if(gamma && c<3){const double exponent=100000.0/(2.2*gamma);expected=unsigned(std::clamp(std::pow(expected/255.0,exponent)*255.0+.5,0.0,255.0));}assert(unsigned(l.bytes[std::size_t(y)*l.pitch+x*4+c])==expected);++oracle_pixels;}
  }
  // Correct both supplied levels, then generate from the corrected second level.
  auto supplied=Make(9,7,13,45000);
  auto second=Make(4,3,13);
  supplied.levels.push_back(second.levels.front());
  Observe(supplied,[&]{supplied.PrepareMips(mg::TextureMipPolicy::CompleteBox);});
  assert(supplied.levels.size()==5 && supplied.png_gamma==0);
  const auto& input=second.levels.front();
  const auto& corrected=supplied.levels[1];
  const auto& generated=supplied.levels[2];
  assert(corrected.width==8 && corrected.height==4);
  assert(generated.width==4 && generated.height==2);
  const double exponent=100000.0/(2.2*45000u);
  for(unsigned y=0;y<corrected.height;++y) {
    for(unsigned x=0;x<corrected.width;++x) {
      for(unsigned c=0;c<4;++c) {
        unsigned expected=x<input.width && y<input.height
            ? unsigned(input.bytes[std::size_t(y)*input.pitch+x*4+c])
            : supplied.clear_pixel[c];
        if(c<3) expected=unsigned(std::clamp(std::pow(expected/255.0,exponent)*255.0+.5,0.0,255.0));
        assert(unsigned(corrected.bytes[std::size_t(y)*corrected.pitch+x*4+c])==expected);
        ++oracle_pixels;
      }
    }
  }
  for(unsigned y=0;y<generated.height;++y) {
    for(unsigned x=0;x<generated.width;++x) {
      for(unsigned c=0;c<4;++c) {
        const auto offset=std::size_t(y)*2*corrected.pitch+x*8+c;
        const unsigned sum=unsigned(corrected.bytes[offset])+unsigned(corrected.bytes[offset+4])+
            unsigned(corrected.bytes[offset+corrected.pitch])+unsigned(corrected.bytes[offset+corrected.pitch+4]);
        assert(unsigned(generated.bytes[std::size_t(y)*generated.pitch+x*4+c])==(sum+2)/4);
        ++oracle_pixels;
      }
    }
  }
  for(auto format:{mg::TextureFormat::BC1,mg::TextureFormat::BC2,mg::TextureFormat::BC3})for(unsigned w:{1u,4u,5u,9u})for(unsigned h:{1u,3u,5u})for(auto policy:{mg::TextureMipPolicy::OneLevel,mg::TextureMipPolicy::CompleteBox}){auto t=Make(w,h,16,0,format);Observe(t,[&]{t.PrepareMips(policy);});}
  for(unsigned budget:{0u,1u,512u,4096u,64u*1024*1024+1})for(unsigned count:{0u,1u,3u,32u}){auto t=Make(9,7,3,45000);Observe(t,[&]{t.PrepareMips(mg::TextureMipPolicy::CompleteBox,budget,count);});}
  for(unsigned kind=0;kind<6;++kind){auto t=Make(9,7,3);if(kind==0)t.levels.clear();if(kind==1)t.levels[0].width=0;if(kind==2)t.levels[0].pitch=1;if(kind==3)t.levels[0].bytes.resize(1);if(kind==4)t.format=mg::TextureFormat(99);if(kind==5)t.png_gamma=45000,t.format=mg::TextureFormat::BC1;Observe(t,[&]{t.PrepareMips(mg::TextureMipPolicy::CompleteBox);});}
 }
 assert(std::fesetround(previous)==0);std::printf("MIP_COPY cases=%llu rejected=%llu checksum=%llu oracle_channels=%llu full_output padding_clear_gamma BC123_real_codec box_1D_2D fenv4 budget_failures_transactional CPU_ONLY_NOT_GPU\n",(unsigned long long)cases,(unsigned long long)rejected,(unsigned long long)hash,(unsigned long long)oracle_pixels);
}
