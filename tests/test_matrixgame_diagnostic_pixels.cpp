#include "native_diagnostic_pixels.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <limits>
namespace d=srhd_awa::matrixgame::diagnostic;
template<class F>void Reject(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}assert(caught);}
int main(){
    constexpr unsigned w=1280,h=720;std::vector<std::byte> pixels(std::size_t(w)*h*4);
    const auto center=(std::size_t(h/2)*w+w/2)*4;
    Reject([&]{d::CheckSmokePixels(w,h,pixels);}); // An empty/clear frame cannot pass.
    pixels[center+2]=pixels[center+3]=std::byte{255};d::CheckSmokePixels(w,h,pixels);
    pixels[center]=std::byte{255};pixels[center+2]=std::byte{0};Reject([&]{d::CheckSmokePixels(w,h,pixels);}); // Wrong channel order.
    pixels[center]=std::byte{0};pixels[center+2]=std::byte{255};pixels[center+3]=std::byte{0};Reject([&]{d::CheckSmokePixels(w,h,pixels);}); // Transparent marker.
    pixels[center+3]=std::byte{255};Reject([&]{d::CheckSmokePixels(w,h,std::span{pixels}.first(pixels.size()-1));});
    Reject([&]{d::CheckSmokePixels(0,h,pixels);});Reject([&]{d::ValidateFrame(std::numeric_limits<unsigned>::max(),h,pixels);});
    const auto quad=d::SmokeQuad(w,h);assert(quad.size()==4 && quad[0].x<float(w/2) && quad[3].x>float(w/2) && quad[0].y<float(h/2) && quad[3].y>float(h/2));
    std::vector<std::byte> asymmetric(8*8*4),flipped(asymmetric.size());
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){const auto i=(y*8+x)*4;asymmetric[i]=std::byte(17*x);asymmetric[i+1]=std::byte(23*y);asymmetric[i+2]=std::byte(x+31*y);asymmetric[i+3]=std::byte{255};}
    d::CheckIdentityPresentation(8,8,asymmetric,asymmetric);
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)for(unsigned c=0;c<4;++c)flipped[(y*8+x)*4+c]=asymmetric[((7-y)*8+x)*4+c];
    Reject([&]{d::CheckIdentityPresentation(8,8,asymmetric,flipped);});
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x)for(unsigned c=0;c<4;++c)flipped[(y*8+x)*4+c]=asymmetric[(y*8+7-x)*4+c];
    Reject([&]{d::CheckIdentityPresentation(8,8,asymmetric,flipped);});
    flipped=asymmetric;flipped[3]=std::byte{254};Reject([&]{d::CheckIdentityPresentation(8,8,asymmetric,flipped);});
    std::puts("DIAGNOSTIC_PIXELS PASS clear_only_rejected wrong_channels_rejected alpha_rejected truncated_overflow_rejected identity_orientation XY_flip_rejected CPU_CHECKER_ONLY NO_GPU_RENDER");
}
