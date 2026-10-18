// SPDX-License-Identifier: GPL-2.0-or-later
#include "texture_asset.hpp"
#include "jpeg_texture.hpp"
#include "../port/matrixgame/deko/texture_asset_upload.hpp"
#include "CFile.hpp"
#include "config_unicode.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace srhd_awa::matrixgame;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"CHECK %d: %s\n",__LINE__,#x);std::exit(3);}}while(0)
void Write(const void* p,std::size_t n){CHECK(std::fwrite(p,1,n,stdout)==n);}
template<class... T>void Words(T... value){const std::array<std::uint32_t,sizeof...(T)> words{std::uint32_t(value)...};Write(words.data(),words.size()*4);}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}CHECK(rejected);}
void Controls() {
  TextureAsset a;a.levels.push_back({7,5,31,std::vector<std::byte>(31*5,std::byte{42})});
  const auto plan=PlanTextureAssetUpload(a);CHECK(plan.bytes==160&&plan.levels[0].row_bytes==28);
  std::vector<std::byte> stage(plan.bytes+16,std::byte{0xcd});PackTextureAsset(a,stage);
  for(unsigned y=0;y<5;++y){for(unsigned x=0;x<28;++x)CHECK(stage[y*32+x]==std::byte{42});for(unsigned x=28;x<32;++x)CHECK(stage[y*32+x]==std::byte{});}
  for(unsigned i=plan.bytes;i<stage.size();++i)CHECK(stage[i]==std::byte{0xcd});
  Reject([&]{PackTextureAsset(a,{stage.data(),plan.bytes-1});});
  a.levels[0].bytes.resize(plan.bytes+16,std::byte{42});
  Reject([&]{PackTextureAsset(a,a.levels[0].bytes);});
  a.levels[0].bytes.pop_back();a.levels[0].pitch=100;Reject([&]{PlanTextureAssetUpload(a);});
  a.levels[0].pitch=31;a.levels.push_back({4,2,16,std::vector<std::byte>(32)});Reject([&]{PlanTextureAssetUpload(a);});
  a.levels.pop_back();a.format=static_cast<TextureFormat>(99);Reject([&]{PlanTextureAssetUpload(a);});
}
int main(int argc,char** argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout),_O_BINARY);
#endif
  try {
    if(argc==3&&std::string(argv[1])=="--jpeg-codec-limit") {
      std::ifstream file(std::filesystem::u8path(argv[2]),std::ios::binary);CHECK(bool(file));
      const std::vector<char> source{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
      bool bounded=false;try{DecodeJpegTexture(std::as_bytes(std::span(source)));}catch(const std::length_error&){bounded=true;}
      CHECK(bounded);return 0;
    }
    if(argc!=2&&argc!=3)return 2;Controls();Base::CFile::StaticInit();
    if(argc==3){Base::CFile::AddPackFile(::matrixgame::native::ConfigWide(argv[2]).c_str());Base::CFile::OpenPackFiles();}
    std::ifstream manifest(std::filesystem::u8path(argv[1]));CHECK(bool(manifest));std::string name;
    while(std::getline(manifest,name)) {
      TextureAsset asset;bool ok=false;
      try{asset=TextureAsset::Load(::matrixgame::native::ConfigWide(name));ok=true;}catch(...){}
      if(!ok){Words(0,0,0);continue;}
      // Independently exercise memory input; the source bytes must stay immutable.
      Base::CFile source{::matrixgame::native::ConfigWide(name)};source.OpenRead();
      std::vector<std::byte> input(std::size_t(source.Size()));source.Read(input.data(),input.size());const auto preserved=input;
      const auto memory=TextureAsset::Decode(input);CHECK(input==preserved&&memory.format==asset.format&&memory.levels.size()==asset.levels.size());
      if(input.size()>2&&input[0]==std::byte{255}&&input[1]==std::byte{216}) {
        bool bounded=false;try{DecodeJpegTexture(input,8);}catch(const std::length_error&){bounded=true;}
        CHECK(bounded&&input==preserved);
      }
      const auto plan=PlanTextureAssetUpload(asset);std::vector<std::byte> packed(plan.bytes+16,std::byte{0xcd});PackTextureAsset(asset,packed);
      for(std::size_t i=plan.bytes;i<packed.size();++i)CHECK(packed[i]==std::byte{0xcd});
      Words(1,asset.format,asset.levels.size());
      for(std::size_t i=0;i<asset.levels.size();++i) {
        const auto& level=asset.levels[i];const auto& p=plan.levels[i];const auto& other=memory.levels[i];
        CHECK(level.width==other.width&&level.height==other.height&&level.pitch==other.pitch&&level.bytes==other.bytes);
        Words(level.width,level.height,level.pitch,level.bytes.size(),p.offset,p.pitch,p.rows);Write(level.bytes.data(),level.bytes.size());
      }
      Words(plan.bytes);Write(packed.data(),plan.bytes);
    }
    CHECK(!std::fflush(stdout));return 0;
  }catch(...){std::fprintf(stderr,"texture contract failed\n");return 2;}
}
