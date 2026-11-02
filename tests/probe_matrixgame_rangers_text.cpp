// SPDX-License-Identifier: GPL-2.0-or-later
// Real original Robot/CacheFont/AFT rasterizer, installed data; no GUI/Init/GPU.
#include "runtime_support.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_File.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/GR_Main.hpp"
#include "units/GlobalsV.hpp"
#include "units/Robot.hpp"
#include "units/aPacket.hpp"
#include "units/EC_HsFile.hpp"
#include "rangers_host_callbacks.hpp"
#include "rangers_audio_session.hpp"
#include "rangers_bridge.hpp"
#include "ec_file_adapter.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
namespace m=srhd_awa::matrixgame;
struct Packages {
 Packages(){
  assert(!EC_HsFile::PackageCollection);assert(aPacket::InitializePackageCollection());
  try{
   auto* install=pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
   try{
    char16_t install_path[]=u"INSTALL.TXT";
    install->LoadFromTextFileWithEncodingProbe(install_path,false);auto* block=install->GetBlock(u"Packages"sv);
    for(int i=0;i<block->GetParamCount();++i){auto* pack=pas::construct_call<EC_HsFile::TPackFileEC>(EC_HsFile::TPackFileEC_Create);pack->SetPackagePath(static_cast<pas::AnsiString>(block->GetParamValue(i)));EC_HsFile::PackageCollection->AddPackToBack(pack);}
   }catch(...){pas::free(install);throw;}
   pas::free(install);
   EC_HsFile::PackageCollection->CloseAllPackages();assert(EC_HsFile::PackageCollection->OpenAllPackages());
  }catch(...){aPacket::FinalizePackageCollection();throw;}
 }
 ~Packages(){aPacket::FinalizePackageCollection();}
};
std::vector<unsigned char> Pixels(const m::TextImage& image){
 assert(image.id.value && image.pixels && image.width>0 && image.height>0 && image.pitch>=image.width*4);
 std::vector<unsigned char> pixels;for(int y=0;y<image.height;++y){auto* row=static_cast<const unsigned char*>(image.pixels)+y*image.pitch;pixels.insert(pixels.end(),row,row+image.width*4);}return pixels;
}
int main(int argc,char** argv){
 assert(argc==3 || (argc==4 && !std::strcmp(argv[3],"--baseline-null")));
 std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);Packages packages;
 GlobalsV::HardwareRenderingEnabled=false;GlobalsV::SoundEnabled=true;GlobalsV::MusicEnabled=false;
 auto* root=pas::construct_call<EC_Data::TDataEC>(EC_Data::TDataEC_Create);
 try{
  root->LoadFromEncryptedDatFile(u"CFG\\CacheData.dat"_w);auto* font_entry=root->FindEntryByPath(u"Font.2Normal"_w);assert(font_entry);
  auto font_file=static_cast<pas::AnsiString>(font_entry->SharedFileRef->FileRef->GetFileName());std::fprintf(stderr,"RANGERS_TEXT font_file=%s offset=%u bytes=%d\n",font_file.c_str(),font_entry->FileOffset,font_entry->ByteCount);
  {
   m::RangersAudioSession cache_owner(*root);
   m::RangersBridge bridge(m::MakeRangersHostCallbacks());
   m::TextRequest request{};request.text=u"MatrixGame Привет 123";request.font=u"Font.2Normal";request.color=0xff40a0e0;
   request.width=200;request.height=64;request.clip={0,0,200,64};
   auto control=bridge.RenderText(request);auto full=Pixels(control);assert(std::count_if(full.begin(),full.end(),[](auto v){return v!=0;})>100);bridge.FreeText(control.id);
   std::puts("RANGERS_TEXT original_AFT_explicit_clip=PASS");std::fflush(stdout);
   if(argc==4){request.has_clip=false;auto image=bridge.RenderText(request);bridge.FreeText(image.id);std::puts("BASELINE unexpectedly accepted null clip");return 7;}
   auto original=m::MakeRangersHostCallbacks();original.RenderText=Robot::TRobotRenderText(Robot::RobotRenderText);m::RangersBridge reference(original);
   std::ofstream observed("pixels.bin",std::ios::binary);unsigned cases{},clipped{},automatic{};
   for(int wrap:{0,1})for(int ax:{0,1,2,3})for(int ay:{0,1,2}){
    request.wrap=wrap;request.align_x=ax;request.align_y=ay;request.has_clip=true;
    auto expected=reference.RenderText(request);auto expected_pixels=Pixels(expected);
    request.has_clip=false;auto actual=bridge.RenderText(request);auto pixels=Pixels(actual);
    assert(actual.width==expected.width && actual.height==expected.height && pixels==expected_pixels);
    observed.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());reference.FreeText(expected.id);bridge.FreeText(actual.id);++cases;
   }
   request.wrap=0;request.align_x=0;request.align_y=0;request.has_clip=true;request.clip={0,0,64,64};
   auto crop=bridge.RenderText(request);auto cropped=Pixels(crop);
   for(int y=0;y<64;++y)for(int x=0;x<200;++x)for(int c=0;c<4;++c)assert(cropped[(y*200+x)*4+c]==(x<64?full[(y*200+x)*4+c]:0));
   assert(cropped!=full);bridge.FreeText(crop.id);++clipped;
   request.width=0;request.height=0;request.clip={0,0,0,0};
   auto expected=reference.RenderText(request);auto expected_pixels=Pixels(expected);request.has_clip=false;
   auto measured=bridge.RenderText(request);assert(Pixels(measured)==expected_pixels && measured.width==expected.width && measured.height==expected.height);
   std::printf("RANGERS_TEXT measured=%dx%d\n",measured.width,measured.height);reference.FreeText(expected.id);bridge.FreeText(measured.id);++automatic;
   // Render errors must not leave a published native image handle.
   request.width=0;request.wrap=1;bool rejected=false;try{bridge.RenderText(request);}catch(const std::exception&){rejected=true;}assert(rejected);
   bridge.Reset();
   std::printf("RANGERS_TEXT exact_original_pairs=%u crop=%u automatic=%u error_cleanup=PASS real_Font_2Normal=PASS CPU_AFT_RASTER_ONLY NO_GUI NO_MATRIX_INIT NO_SWITCH\n",cases,clipped,automatic);
  }
  assert(!GR_Main::GlobalCache && !GR_Main::SoundManager && !GR_Main::CacheDataRoot);pas::free(root);root=nullptr;
 }catch(...){if(root)pas::free(root);throw;}
}
