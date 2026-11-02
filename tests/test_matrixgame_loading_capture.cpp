// Snapshot lifecycle/format oracle only: CPU pixel fixture, no GPU success claim.
#include "transition_backend.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
using namespace srhd_awa::matrixgame;
namespace n=native_form;namespace t=transition;
struct Source final:n::ScreenshotProvider {
 n::ScreenshotPixels image;unsigned calls{};bool fail{};
 n::ScreenshotPixels CaptureScreenshot()override{++calls;if(fail)throw std::runtime_error("capture failed");return image;}
};
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
unsigned Channel(unsigned x,unsigned y,unsigned c){return (x*37+y*53+c*79+17)&255;}
Source Pixels(n::ScreenshotFormat format){
 Source source;auto& image=source.image;const unsigned bpp=format==n::ScreenshotFormat::Rgba8?4:2;
 image={7,5,7*bpp+3,format,{}};image.bytes.resize(image.pitch*image.height,std::byte{0xa5});
 for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x){
  auto* p=image.bytes.data()+y*image.pitch+x*bpp;const auto r=Channel(x,y,0),g=Channel(x,y,1),b=Channel(x,y,2);
  if(bpp==4){p[0]=std::byte(r);p[1]=std::byte(g);p[2]=std::byte(b);p[3]=std::byte{0x31};}
  else {const unsigned packed=((r>>3)<<11)|((g>>2)<<5)|(b>>3);p[0]=std::byte(packed&255);p[1]=std::byte(packed>>8);}
 }
 return source;
}
int main(){
 t::Bitmap bitmap;Reject([&]{t::CaptureLoadingImage(bitmap,7,5);});
 unsigned cases{};
 for(auto format:{n::ScreenshotFormat::Rgba8,n::ScreenshotFormat::Rgb565})for(int cycle=0;cycle<2;++cycle){
  auto source=Pixels(format);
  {
   t::LoadingCaptureScope scope(source);assert(source.calls==1);
   Reject([&]{t::LoadingCaptureScope duplicate(source);});assert(source.calls==1);
   // Simulate slot invalidation/reset after capture. The saved frame must not
   // borrow mutable provider storage or perform a late second readback.
   std::fill(source.image.bytes.begin(),source.image.bytes.end(),std::byte{0});source.fail=true;
   Reject([&]{t::CaptureLoadingImage(bitmap,8,5);});Reject([&]{t::CaptureLoadingImage(bitmap,0,5);});
   t::CaptureLoadingImage(bitmap,7,5);assert(source.calls==1 && bitmap.SizeX()==7 && bitmap.SizeY()==5 && bitmap.BytePP()==3);
   for(unsigned y=0;y<5;++y)for(unsigned x=0;x<7;++x)for(unsigned c=0;c<3;++c){
    unsigned expected=Channel(x,y,c);
    if(format==n::ScreenshotFormat::Rgb565){const unsigned bits=c==1?6:5;expected>>=8-bits;expected=(expected<<(8-bits))|(expected>>(2*bits-8));}
    assert(bitmap.Data()[y*bitmap.Pitch()+x*3+c]==expected);
   }
   const auto* pointer=bitmap.Data();Reject([&]{t::CaptureLoadingImage(bitmap,7,5);});assert(bitmap.Data()==pointer);++cases;
  }
  Reject([&]{t::CaptureLoadingImage(bitmap,7,5);});
 }
 for(int failure=0;failure<5;++failure){
  auto bad=Pixels(n::ScreenshotFormat::Rgba8);
  if(failure==0)bad.fail=true;
  if(failure==1)bad.image.width=0;
  if(failure==2)bad.image.pitch=27;
  if(failure==3)bad.image.bytes.pop_back();
  if(failure==4)bad.image.format=static_cast<n::ScreenshotFormat>(99);
  Reject([&]{t::LoadingCaptureScope scope(bad);});
  auto good=Pixels(n::ScreenshotFormat::Rgba8);{t::LoadingCaptureScope retry(good);t::CaptureLoadingImage(bitmap,7,5);}
 }
 std::printf("LOADING_CAPTURE formats_cycles=%u owned_snapshot=PASS prior_slot_reset=PASS pitch_rgb565_rgba=PASS one_shot=PASS failures_retry=5 cleanup=PASS SYNTHETIC_CPU_PIXELS NO_GPU\n",cases);
}
