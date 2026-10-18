// SPDX-License-Identifier: GPL-2.0-or-later
#include "texture_asset.hpp"
#include "CFile.hpp"
#include "config_unicode.hpp"
#include <algorithm>
#include "BC.h"
#include <array>
#include <cstdio>
#include <fstream>
#include <cmath>
#include <cfenv>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace srhd_awa::matrixgame;
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"mip CHECK %d: %s\n",__LINE__,#e);return 3;}}while(0)
int main(int argc,char** argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout),_O_BINARY);
#endif
  try {
    if(argc==4&&std::string_view(argv[1])=="--encode-bc-block") {
      const auto kind=std::stoi(argv[2]);CHECK(kind>=1&&kind<=3);
      std::array<float,64> raw{};std::ifstream file(argv[3],std::ios::binary);
      CHECK(bool(file.read(reinterpret_cast<char*>(raw.data()),sizeof(raw)))&&file.peek()==EOF);
      alignas(16) DirectX::XMVECTOR pixels[16];
      for(unsigned i=0;i<64;++i)CHECK(std::isfinite(raw[i])&&raw[i]>=0&&raw[i]<=1);
      for(unsigned i=0;i<16;++i)pixels[i]=DirectX::XMVectorSet(raw[i*4],raw[i*4+1],raw[i*4+2],raw[i*4+3]);
      std::array<std::uint8_t,16> encoded{};
      if(kind==1)DirectX::D3DXEncodeBC1(encoded.data(),pixels,.5f,DirectX::BC_FLAGS_NONE);
      else if(kind==2)DirectX::D3DXEncodeBC2(encoded.data(),pixels,DirectX::BC_FLAGS_NONE);
      else DirectX::D3DXEncodeBC3(encoded.data(),pixels,DirectX::BC_FLAGS_NONE);
      CHECK(std::fwrite(encoded.data(),1,kind==1?8:16,stdout)==unsigned(kind==1?8:16));return 0;
    }
    // Every encoder path must restore this thread's caller rounding mode,
    // including constant blocks and BC3's early opaque-alpha return.
    const int caller_rounding=std::fegetround();CHECK(caller_rounding!=-1);
    for(const auto mode:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}) {
      for(unsigned pattern=0;pattern<3;++pattern) {
        alignas(16) DirectX::XMVECTOR pixels[16];
        for(unsigned i=0;i<16;++i) {
          const float v=pattern==0?0.f:pattern==1?1.f:float(i)/15.f;
          pixels[i]=DirectX::XMVectorSet(v,v,v,v);
        }
        for(unsigned kind=1;kind<=3;++kind) {
          CHECK(std::fesetround(mode)==0);
          std::array<std::uint8_t,16> encoded{};
          if(kind==1)DirectX::D3DXEncodeBC1(encoded.data(),pixels,.5f,DirectX::BC_FLAGS_NONE);
          else if(kind==2)DirectX::D3DXEncodeBC2(encoded.data(),pixels,DirectX::BC_FLAGS_NONE);
          else DirectX::D3DXEncodeBC3(encoded.data(),pixels,DirectX::BC_FLAGS_NONE);
          CHECK(std::fegetround()==mode);
          CHECK(std::fesetround(caller_rounding)==0);
        }
      }
    }
    TextureAsset control;control.levels.push_back({4,4,16,std::vector<std::byte>(64,std::byte{37})});
    auto* original=control.levels[0].bytes.data();const auto bytes=control.levels[0].bytes;bool rejected=false;
    try{control.PrepareMips(TextureMipPolicy::CompleteBox,64);}catch(const std::length_error&){rejected=true;}
    CHECK(rejected&&control.levels.size()==1&&control.levels[0].bytes==bytes&&control.levels[0].bytes.data()==original);
    control.PrepareMips(TextureMipPolicy::CompleteBox);CHECK(control.levels.size()==3&&control.levels[0].bytes.data()==original);
    for(const auto& level:control.levels)for(const auto b:level.bytes)CHECK(b==std::byte{37});
    control.PrepareMips(TextureMipPolicy::OneLevel);CHECK(control.levels.size()==1&&control.levels[0].bytes.data()==original);
    control.png_gamma=45000;
    control.PrepareMips(TextureMipPolicy::PreserveSupplied);
    CHECK(control.png_gamma==45000&&control.levels[0].bytes==bytes);
    rejected=false;
    try{control.PrepareMips(TextureMipPolicy::OneLevel,64);}catch(const std::length_error&){rejected=true;}
    CHECK(rejected&&control.png_gamma==45000&&control.levels[0].bytes==bytes&&control.levels[0].bytes.data()==original);
    control.PrepareMips(TextureMipPolicy::OneLevel);
    CHECK(control.png_gamma==0&&control.levels[0].bytes[0]==std::byte{36}&&control.levels[0].bytes[3]==std::byte{37});
    original=control.levels[0].bytes.data();
    control.PrepareMips(TextureMipPolicy::OneLevel);
    CHECK(control.levels[0].bytes.data()==original&&control.levels[0].bytes[0]==std::byte{36});
    bool single=argc>1&&std::string_view(argv[1])=="--single";const int index=single?2:1;
    if(argc!=index+1&&argc!=index+2)return 2;
    Base::CFile::StaticInit();if(argc==index+2){const auto p=::matrixgame::native::ConfigWide(argv[index+1]);Base::CFile::AddPackFile(p.c_str());Base::CFile::OpenPackFiles();}
    std::ifstream manifest(argv[index]);CHECK(bool(manifest));std::string name;
    while(std::getline(manifest,name)) {
      auto image=TextureAsset::Load(::matrixgame::native::ConfigWide(name));
      image.PrepareMips(single?TextureMipPolicy::OneLevel:TextureMipPolicy::CompleteBox);
      const std::array<std::uint32_t,2> header{std::uint32_t(image.format),std::uint32_t(image.levels.size())};CHECK(std::fwrite(header.data(),1,sizeof(header),stdout)==sizeof(header));
      for(const auto& level:image.levels) {
        const auto pitch=image.format==TextureFormat::RGBA8?level.width*4:((level.width+3)/4)*(image.format==TextureFormat::BC1?8:16);
        const auto rows=image.format==TextureFormat::RGBA8?level.height:(level.height+3)/4;
        const std::array<std::uint32_t,3> shape{level.width,level.height,pitch*rows};CHECK(std::fwrite(shape.data(),1,sizeof(shape),stdout)==sizeof(shape));
        for(unsigned y=0;y<rows;++y)CHECK(std::fwrite(level.bytes.data()+std::size_t(y)*level.pitch,1,pitch,stdout)==pitch);
      }
    }
    CHECK(!std::fflush(stdout));return 0;
  }catch(const std::exception& error){std::fprintf(stderr,"mip error: %s\n",error.what());return 2;}
  catch(...){std::fprintf(stderr,"mip exception\n");return 2;}
}
