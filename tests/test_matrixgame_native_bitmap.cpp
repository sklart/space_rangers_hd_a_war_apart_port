// SPDX-License-Identifier: GPL-2.0-or-later
// Actual CBitmap/FilePNG/PKG execution; stdout is a binary pixel transcript.
#include "CBitmap.hpp"
#include "FilePNG.hpp"
#include "CFile.hpp"
#include "CException.hpp"
#include "config_unicode.hpp"
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK %d: %s\n",__LINE__,#e);std::exit(3);}}while(0)
namespace fs=std::filesystem;
static_assert(!std::is_copy_constructible_v<CBitmap> && std::is_nothrow_move_constructible_v<CBitmap>);
#ifdef MATRIXGAME_BITMAP_FAULT_INJECTION
std::size_t fail_size=0;unsigned failed_allocations=0;
extern "C" void* __real_calloc(std::size_t,std::size_t);
extern "C" void* __real_realloc(void*,std::size_t);
extern "C" void* __wrap_calloc(std::size_t count,std::size_t size) {
  if(fail_size&&size&&count==fail_size/size&&count*size==fail_size){fail_size=0;++failed_allocations;return nullptr;}
  return __real_calloc(count,size);
}
extern "C" void* __wrap_realloc(void* p,std::size_t size) {
  if(fail_size&&size==fail_size){fail_size=0;++failed_allocations;return nullptr;}
  return __real_realloc(p,size);
}
void AllocationFailure(void* source,int bytes,std::size_t size,bool rgba) {
  CBitmap bitmap;const auto previous=failed_allocations;fail_size=size;bool caught=false;
  try{if(rgba)bitmap.LoadRGBAFromPNG(source,bytes);else bitmap.LoadFromPNG(source,bytes);}catch(const Base::CException&){caught=true;}
  CHECK(caught&&failed_allocations==previous+1&&!fail_size&&!bitmap.Data()&&!bitmap.SizeX());
  CHECK(rgba?bitmap.LoadRGBAFromPNG(source,bytes):bitmap.LoadFromPNG(source,bytes));
}
#endif
void Dump(const CBitmap& b,bool ok) {
  if(!ok)CHECK(!b.Data()&&!b.SizeX()&&!b.SizeY());
  const std::array<std::uint32_t,10> header{ok?1u:0u,std::uint32_t(b.SizeX()),std::uint32_t(b.SizeY()),std::uint32_t(b.BytePP()),
    std::uint32_t(b.BytePP()*8),std::uint32_t(b.Format()),b.ColorMask(0),b.ColorMask(1),b.ColorMask(2),b.ColorMask(3)};
  CHECK(std::fwrite(header.data(),sizeof(header),1,stdout)==1);
  if(ok)for(int y=0;y<b.SizeY();++y)CHECK(std::fwrite(b.Data()+std::size_t(y)*b.Pitch(),std::size_t(b.SizeX())*b.BytePP(),1,stdout)==1);
}
template<class E,class F>void Reject(F f){bool rejected=false;try{f();}catch(const E&){rejected=true;}CHECK(rejected);}
std::wstring Wide(const fs::path& p){return ::matrixgame::native::ConfigWide(p.generic_string());}
void Raster(const fs::path& output) {
  CBitmap rgb,rgba;rgb.CreateRGB(5,3);rgba.CreateRGBA(5,3);
  rgb.Fill({0,0},rgb.Size(),0x00332211);rgba.Fill({0,0},rgba.Size(),0x80402010);
  rgb.MergeWithAlpha({0,0},rgb.Size(),rgba,{0,0});
  for(int y=0;y<3;++y)for(int x=0;x<5;++x){const auto* p=rgb.Data()+y*rgb.Pitch()+x*3;CHECK(p[0]==16&&p[1]==32&&p[2]==57);}
  CBitmap converted;converted.CreateRGBA(5,3);converted.Copy({0,0},rgb.Size(),rgb,{0,0});
  for(int y=0;y<3;++y)for(int x=0;x<5;++x)CHECK(converted.Data()[y*converted.Pitch()+x*4+3]==255);
  CBitmap overlap;overlap.CreateRGBA(6,2);
  for(int y=0;y<2;++y)for(int x=0;x<6;++x)for(int c=0;c<4;++c)overlap.Data()[y*overlap.Pitch()+x*4+c]=std::uint8_t(x+y*11+c);
  const std::vector<std::uint8_t> before(overlap.Data(),overlap.Data()+overlap.Pitch()*2);
  overlap.Copy({1,0},{5,2},overlap,{0,0});
  for(int y=0;y<2;++y)CHECK(!std::memcmp(overlap.Data()+y*overlap.Pitch()+4,before.data()+y*overlap.Pitch(),20));
  std::vector<std::uint8_t> external(69,0xee);
  {
    CBitmap borrowed;borrowed.CreateRGBA(5,3,23,external.data());borrowed.Fill({0,0},borrowed.Size(),0x80402010);
    const auto copy=external;borrowed.Make2xSmaller();CHECK(borrowed.SizeX()==2&&borrowed.SizeY()==1&&borrowed.Pitch()==8);
    CHECK(external==copy); // No realloc/free or writes through a borrowed allocation.
  }
  CBitmap gradient;gradient.CreateRGBA(6,6);
  for(int y=0;y<6;++y)for(int x=0;x<6;++x)for(int c=0;c<4;++c)gradient.Data()[y*gradient.Pitch()+x*4+c]=std::uint8_t(x*11+y*3+c*17);
  CBitmap crop;gradient.Make2xSmaller({2,2},{4,4},crop);
  for(int y=0;y<2;++y)for(int x=0;x<2;++x)for(int c=0;c<4;++c)CHECK(crop.Data()[y*crop.Pitch()+x*4+c]==(2+x*2)*11+(2+y*2)*3+c*17+7);
  auto* retained=gradient.Data();Reject<std::invalid_argument>([&]{gradient.CreateRGB(INT32_MAX,1);});CHECK(gradient.Data()==retained&&gradient.SizeX()==6);
  Reject<std::invalid_argument>([&]{gradient.CreateRGBA(2,2,0,gradient.Data());});
  CBitmap moved=std::move(gradient);CHECK(moved.Data()==retained&&!gradient.Data()&&!gradient.SizeX());
  const auto rgb_file=Wide(output/"rgb.bmp"),rgba_file=Wide(output/"rgba.bmp"),png_file=Wide(output/"rgba.png");
  rgb.SaveInBMP(rgb_file);crop.SaveInBMP(rgba_file);CHECK(crop.SaveInPNG(png_file));
  Base::CBuf dds;crop.SaveInDDSUncompressed(dds);std::ofstream file(output/"rgba.dds",std::ios::binary);file.write(static_cast<const char*>(dds.Get()),dds.Len());CHECK(bool(file));
  const int needed=FilePNG::Write(nullptr,0,crop.Data(),crop.Pitch(),crop.SizeX(),crop.SizeY(),4,0);CHECK(needed>0);
  std::vector<std::uint8_t> encoded(std::size_t(needed)+16,0xcd);
  CHECK(FilePNG::Write(encoded.data(),needed,crop.Data(),crop.Pitch(),crop.SizeX(),crop.SizeY(),4,0)==needed);
  for(std::size_t i=needed;i<encoded.size();++i)CHECK(encoded[i]==0xcd);
  CBitmap roundtrip;CHECK(roundtrip.LoadRGBAFromPNG(encoded.data(),needed));
  for(int y=0;y<2;++y)CHECK(!std::memcmp(roundtrip.Data()+y*roundtrip.Pitch(),crop.Data()+y*crop.Pitch(),8));
#ifdef MATRIXGAME_BITMAP_FAULT_INJECTION
  AllocationFailure(encoded.data(),needed,16,true);
#endif
  std::array<std::uint8_t,16> gray_data{};for(unsigned i=0;i<16;++i)gray_data[i]=std::uint8_t(i);
  const int gray_size=FilePNG::Write(nullptr,0,gray_data.data(),4,4,4,1,0);CHECK(gray_size>0);
  std::vector<std::uint8_t> gray_png(gray_size);CHECK(FilePNG::Write(gray_png.data(),gray_size,gray_data.data(),4,4,4,1,0)==gray_size);
  CBitmap gray,gray_crop;CHECK(gray.LoadFromPNG(gray_png.data(),gray_size));gray.Make2xSmaller({1,1},{2,2},gray_crop);CHECK(gray_crop.BytePP()==1&&gray_crop.Data()[0]==7);
  CBitmap thin;thin.CreateRGBA(1,3);thin.Fill({0,0},thin.Size(),0xaabbccdd);thin.Make2xSmaller();CHECK(thin.SizeX()==1&&thin.SizeY()==1&&thin.Data()[0]==0xdd&&thin.Data()[3]==0xaa);
  // Invalid rectangles do not reach signed additions or unaligned pixel access.
  const auto safe=std::vector<std::uint8_t>(crop.Data(),crop.Data()+crop.Pitch()*crop.SizeY());
  crop.Fill({INT32_MAX,0},{2,1},1);crop.SwapByte({-1,0},{1,1},0,1);
  crop.Tile({0,0},{2,2},crop,{0,0},{0,1});
  CHECK(!std::memcmp(crop.Data(),safe.data(),safe.size()));
}
int main(int argc,char** argv) {
  if(argc!=3&&argc!=4)return 2;
#ifdef _WIN32
  _setmode(_fileno(stdout),_O_BINARY);
#endif
  try {
    const auto output=fs::u8path(argv[2]);fs::create_directories(output);Raster(output);
    std::ifstream manifest(fs::u8path(argv[1]));CHECK(bool(manifest));Base::CFile::StaticInit();
    if(argc==4){const auto p=::matrixgame::native::ConfigWide(argv[3]);Base::CFile::AddPackFile(p.c_str());Base::CFile::OpenPackFiles();}
    std::string name;std::size_t index=0;
    while(std::getline(manifest,name)) {
      if(name.empty())continue;
      const auto path=::matrixgame::native::ConfigWide(name);Base::CBuf input;input.LoadFromFile(path);
      std::vector<std::uint8_t> unchanged(input.Len());if(input.Len())std::memcpy(unchanged.data(),input.Get(),input.Len());
      CBitmap legacy,rgba,from_file;
      const bool old=legacy.LoadFromPNG(input.Get(),int(input.Len()));Dump(legacy,old);
      const bool converted=rgba.LoadRGBAFromPNG(input.Get(),int(input.Len()));Dump(rgba,converted);
#ifdef MATRIXGAME_BITMAP_FAULT_INJECTION
      if(old&&legacy.Format()==2&&failed_allocations==1) {
        std::uint32_t w,h,n,f;auto id=FilePNG::ReadStart_Buf(input.Get(),std::uint32_t(input.Len()),&w,&h,&n,&f);CHECK(id&&n);FilePNG::ReadCancel(id);
        AllocationFailure(input.Get(),int(input.Len()),std::size_t(n)*4,false);
      }
#endif
      CHECK(from_file.LoadFromPNG(path)==old);
      if(old){CHECK(from_file.SizeX()==legacy.SizeX()&&from_file.SizeY()==legacy.SizeY()&&from_file.BytePP()==legacy.BytePP());for(int y=0;y<legacy.SizeY();++y)CHECK(!std::memcmp(from_file.Data()+y*from_file.Pitch(),legacy.Data()+y*legacy.Pitch(),std::size_t(legacy.SizeX())*legacy.BytePP()));}
      CHECK(input.Len()==unchanged.size()&&(!input.Len()||!std::memcmp(input.Get(),unchanged.data(),input.Len())));
      if(index==0&&converted) {
        std::uint32_t w,h,n,format;const auto id=FilePNG::ReadStart_RGBA(input.Get(),std::uint32_t(input.Len()),&w,&h,&n,&format);CHECK(id);
        if(sizeof(void*)>4)CHECK(id>UINT32_MAX&&std::uintptr_t(std::uint32_t(id))!=id);
        FilePNG::ReadCancel(id);FilePNG::ReadCancel(0);
        const auto short_row=FilePNG::ReadStart_RGBA(input.Get(),std::uint32_t(input.Len()),&w,&h,&n,&format);CHECK(short_row);
        std::vector<std::uint8_t> guard(std::size_t(w)*h*4,0xcd);CHECK(!FilePNG::Read(short_row,guard.data(),w*4-1,nullptr));
        CHECK(std::all_of(guard.begin(),guard.end(),[](auto v){return v==0xcd;}));
      }
      ++index;
    }
#ifdef MATRIXGAME_BITMAP_FAULT_INJECTION
    CHECK(failed_allocations==2);
#endif
    Base::CFile::ReleasePackFiles();CHECK(std::fflush(stdout)==0);return 0;
  }catch(const Base::CException& e){const auto s=::matrixgame::native::ConfigUtf8(e.Info());std::fprintf(stderr,"%s\n",s.c_str());return 2;}
   catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 2;}
}
