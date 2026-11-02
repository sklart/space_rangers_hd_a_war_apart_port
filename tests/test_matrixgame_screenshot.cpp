// SPDX-License-Identifier: GPL-2.0-or-later
// Synthetic capture provider; real original CBitmap PNG/CFile and output paths.
#include "native_screenshot.hpp"
#include "output_paths_backend.hpp"
#include "platform_3g.hpp"
#include "CBitmap.hpp"
#include <cassert>
#include <codecvt>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <locale>
namespace n=srhd_awa::matrixgame::native_form;
namespace p=srhd_awa::matrixgame::platform3g;
namespace o=srhd_awa::matrixgame::output_paths;
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
struct Device final:p::Device,n::ScreenshotProvider {
 n::ScreenshotPixels image;unsigned calls{};bool fail{};
 n::ScreenshotPixels CaptureScreenshot()override{++calls;if(fail)throw std::runtime_error("injected capture failure");return image;}
 p::DeviceCaps ReadCaps()override{throw p::Unsupported("screenshot fixture has no GPU caps");}
 std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("screenshot fixture has no GPU memory");}
 p::GammaRamp ReadGamma(unsigned)override{throw p::Unsupported("screenshot fixture has no gamma");}
 void RestoreGamma(unsigned,const p::GammaRamp&)noexcept override{std::terminate();}
};
int main(int argc,char** argv){
 assert(argc==2);const auto root=std::filesystem::u8path(argv[1]);std::filesystem::create_directories(root);
 auto wide=[](const std::filesystem::path& path){auto bytes=path.u8string();std::string text(bytes.begin(),bytes.end());return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(text);};
 Device device;auto& state=p::Current();assert(!state.ready);auto* previous=state.device;state.device=&device;state.ready=true;
 Reject([&]{n::Screenshot();});assert(!device.calls);
 n::ScreenshotFrame frame;Reject([&]{frame.Require(false,true);});
 unsigned slots{};for(int slot:{2,0,1,2}){frame.Presented(slot,3);assert(frame.Require(false,true)==slot);Reject([&]{frame.Require(true,true);});Reject([&]{frame.Require(false,false);});frame.Invalidate();Reject([&]{frame.Require(false,true);});++slots;}
 Reject([&]{frame.Presented(3,3);});Reject([&]{frame.Require(false,true);});Reject([&]{frame.Presented(-1,3);});
 const auto output=root/"private";const auto output_wide=wide(output);
 unsigned cases{},rejected{},channels{};
 std::ofstream observations(root/"pixels.bin",std::ios::binary);assert(observations);
 {
  o::NativeScope binding(output_wide.c_str());
  const auto folder=output/"SpaceRangersHD/Screenshots";std::filesystem::create_directories(folder);
  const auto sentinel=folder/"existing.png";{std::ofstream file(sentinel);file<<"KEEP";}
  for(auto format:{n::ScreenshotFormat::Rgba8,n::ScreenshotFormat::Rgb565})for(unsigned width:{1u,3u,17u})for(unsigned height:{1u,4u,7u})for(unsigned padding:{0u,8u}){
   const unsigned bpp=format==n::ScreenshotFormat::Rgba8?4:2;
   device.image={width,height,width*bpp+padding,format,{}};device.image.bytes.resize(device.image.pitch*height,std::byte{0xa5});
   std::vector<unsigned char> expected;
   for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
    auto* pixel=device.image.bytes.data()+y*device.image.pitch+x*bpp;
    unsigned r=(x*19+y*37+11)&255,g=(x*73+y*13+67)&255,b=(x*31+y*83+179)&255;
    if(bpp==4){pixel[0]=std::byte(r);pixel[1]=std::byte(g);pixel[2]=std::byte(b);pixel[3]=std::byte((x+y)&255);}
    else {r>>=3;g>>=2;b>>=3;const unsigned packed=(r<<11)|(g<<5)|b;pixel[0]=std::byte(packed&255);pixel[1]=std::byte(packed>>8);r=(r<<3)|(r>>2);g=(g<<2)|(g>>4);b=(b<<3)|(b>>2);}
    expected.insert(expected.end(),{static_cast<unsigned char>(r),static_cast<unsigned char>(g),static_cast<unsigned char>(b)});
   }
   const auto before=device.image.bytes;const auto filename=n::Screenshot();const auto path=o::FilesystemPath(filename);
   assert(path.parent_path()==folder && std::filesystem::file_size(path)>0 && device.image.bytes==before);
   CBitmap decoded;assert(decoded.LoadFromPNG(filename));assert(decoded.SizeX()==int(width) && decoded.SizeY()==int(height) && decoded.BytePP()==3);
   for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width*3;++x){const auto actual=decoded.Data()[y*decoded.Pitch()+x];assert(actual==expected[y*width*3+x]);observations.put(char(actual));++channels;}
   ++cases;
  }
  assert(device.calls==cases);
  auto count=[&]{unsigned files{};for(const auto& entry:std::filesystem::directory_iterator(folder)){(void)entry;++files;}return files;};
  assert(count()==cases+1);const auto valid=device.image;
  for(unsigned bad=0;bad<7;++bad){device.image=valid;
   switch(bad){case 0:device.image.width=0;break;case 1:device.image.height=0;break;case 2:device.image.pitch=1;break;case 3:device.image.bytes.pop_back();break;case 4:device.image.format=static_cast<n::ScreenshotFormat>(99);break;case 5:device.image.width=16385;break;case 6:device.fail=true;break;}
   Reject([&]{n::Screenshot();});++rejected;assert(count()==cases+1);
  }
  device.fail=false;device.image=valid;std::ifstream file(sentinel);std::string text;file>>text;assert(text=="KEEP");
 }
 assert(!o::NativeBound());Reject([&]{n::Screenshot();});
 // Deterministic I/O failure: required parent directory is an existing file.
 const auto blocked=root/"blocked";std::filesystem::create_directories(blocked);{std::ofstream file(blocked/"SpaceRangersHD");file<<"KEEP-PARENT";}
 {const auto text=wide(blocked);o::NativeScope binding(text.c_str());Reject([&]{n::Screenshot();});++rejected;}
 {std::ifstream file(blocked/"SpaceRangersHD");std::string text;file>>text;assert(text=="KEEP-PARENT");}
 state.ready=false;state.device=previous;assert(!o::NativeBound());
 std::printf("SCREENSHOT PNG_roundtrip=%u RGB_channels=%u rejected=%u slot_sequences=%u original_CBitmap_CFile=PASS private_output=PASS existing_files_preserved=PASS CPU_CAPTURE_FIXTURE NO_GPU NO_INIT NO_SWITCH\n",cases,channels,rejected,slots);
}
