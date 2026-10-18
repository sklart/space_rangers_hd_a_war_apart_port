// SPDX-License-Identifier: GPL-2.0-or-later
// The x86 variant executes real CBaseTexture::ParseFlags, without GPU Load.
#include "CBlockPar.hpp"
#include "CFile.hpp"
#include <utils.hpp>
#ifdef MATRIXGAME_ORIGINAL_TEXTURE_REFERENCE
#include "Texture.hpp"
struct InspectTexture : CTexture {std::uint32_t ParsedFlags()const{return m_Flags;}};
#else
#include "texture_request.hpp"
#endif
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace Base;
int main(int argc,char** argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout),_O_BINARY);
#endif
  try {
    if(argc!=2&&argc!=3)return 2;CFile::StaticInit();
    if(argc==3){const auto p=utils::to_wstring(argv[2]);CFile::AddPackFile(p.c_str());CFile::OpenPackFiles();}
    std::ifstream manifest(std::filesystem::u8path(argv[1]));if(!manifest)return 2;std::string text;
    while(std::getline(manifest,text)) {
      std::wstring path;std::uint32_t flags=0,ok=0,loaded=0;
      try {
        const ParamParser name{utils::to_wstring(text)};
#ifdef MATRIXGAME_ORIGINAL_TEXTURE_REFERENCE
        InspectTexture inspector;inspector.ParseFlags(name);flags=inspector.ParsedFlags();
        const auto plain=name.GetStrPar(0,L"?");ok=CFile::FileExist(path,plain.c_str(),L"png~jpg~bmp~dds~strg",false)?1:0;
#else
        using namespace srhd_awa::matrixgame;
        flags=ParseOriginalTextureFlags(name);
        const auto request=TextureRequest::Resolve(name);path=request.path;ok=1;
        try{const auto asset=request.Load();loaded=asset.levels.size();}catch(...){}
#endif
      }catch(...){}
      const auto encoded=utils::from_wstring(path);const std::array<std::uint32_t,3> header{ok,flags,std::uint32_t(encoded.size())};
      if(std::fwrite(header.data(),1,sizeof(header),stdout)!=sizeof(header)||
          std::fwrite(encoded.data(),1,encoded.size(),stdout)!=encoded.size())return 3;
#ifndef MATRIXGAME_ORIGINAL_TEXTURE_REFERENCE
      if(std::fwrite(&loaded,1,4,stdout)!=4)return 3;
#endif
    }
    return std::fflush(stdout)?3:0;
  }catch(...){return 2;}
}
