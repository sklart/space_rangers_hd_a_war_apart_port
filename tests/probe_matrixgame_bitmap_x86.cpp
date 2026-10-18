// SPDX-License-Identifier: GPL-2.0-or-later
// Execute the unchanged original CBitmap/FilePNG in the actual x86 MatrixLib.
#include "CBitmap.hpp"
#include "CException.hpp"
#include "CFile.hpp"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <fcntl.h>
#include <io.h>
int main(int argc,char** argv) {
    _setmode(_fileno(stdout),_O_BINARY);
    try {
        if(argc==4&&std::string(argv[1])=="--dds") {
            CBitmap bitmap;const auto path=std::filesystem::u8path(argv[2]).wstring();
            if(!bitmap.LoadFromPNG(path))return 3;
            Base::CBuf bytes;bitmap.SaveInDDSUncompressed(bytes);
            std::ofstream output(std::filesystem::u8path(argv[3]),std::ios::binary);
            output.write(static_cast<const char*>(bytes.Get()),bytes.Len());return output?0:3;
        }
        if(argc==3&&std::string(argv[1])=="--file-result") {
            CBitmap bitmap;const auto path=std::filesystem::u8path(argv[2]).wstring();
            std::printf("%u",bitmap.LoadFromPNG(path)?1:0);return 0;
        }
        if(argc!=3)return 2;
        std::ifstream manifest(std::filesystem::u8path(argv[1]));if(!manifest)return 2;
        Base::CFile::StaticInit();std::string name;
        while(std::getline(manifest,name)) {
            const auto path=std::filesystem::u8path(name).wstring();Base::CBuf source;source.LoadFromFile(path);
            CBitmap bitmap;const auto ok=bitmap.LoadFromPNG(source.Get(),int(source.Len()));
            std::array<std::uint32_t,10> header{};
            if(ok)header={1,std::uint32_t(bitmap.SizeX()),std::uint32_t(bitmap.SizeY()),std::uint32_t(bitmap.BytePP()),
              std::uint32_t(bitmap.BitPP()),std::uint32_t(bitmap.Format()),bitmap.ColorMask(0),bitmap.ColorMask(1),bitmap.ColorMask(2),bitmap.ColorMask(3)};
            if(std::fwrite(header.data(),sizeof(header),1,stdout)!=1)return 3;
            if(ok)for(int y=0;y<bitmap.SizeY();++y)if(std::fwrite(bitmap.Data()+y*bitmap.Pitch(),std::size_t(bitmap.SizeX())*bitmap.BytePP(),1,stdout)!=1)return 3;
        }
        return std::fflush(stdout)==0?0:3;
    }catch(...){return 2;}
}
