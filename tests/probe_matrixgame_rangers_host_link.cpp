// SPDX-License-Identifier: GPL-2.0-or-later
// Real host services + MatrixGame adapter link root. No native CGame::Init.
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_HsFile.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_Music.hpp"
#include "units/GR_Sound.hpp"
#include "units/GlobalsV.hpp"
#include "units/Robot.hpp"
#include "units/aPacket.hpp"
#include "units/System.hpp"
#include "engine_callbacks.hpp"
#include "rangers_host_callbacks.hpp"
#include "rangers_audio_session.hpp"
#include "rangers_music_session.hpp"
#include "ec_file_adapter.hpp"
#include "e2e4_game_audio_bridge.hpp"
#include <SDL2/SDL.h>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <memory>
namespace m=srhd_awa::matrixgame;
template<class T>struct OriginalDelete{void operator()(T* value)const{pas::free(value);}};
template<class T>using OriginalOwner=std::unique_ptr<T,OriginalDelete<T>>;
struct Packages {
 Packages(){
  assert(!EC_HsFile::PackageCollection);assert(aPacket::InitializePackageCollection());
  try{
   OriginalOwner<EC_BlockPar::TBlockParEC> install(pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create));
   install->LoadFromTextFileWithEncodingProbe(pas::literal_pointer(u"INSTALL.TXT"),false);
   auto* list=install->GetBlock(u"Packages"sv);
   for(int i=0;i<list->GetParamCount();++i){
    OriginalOwner<EC_HsFile::TPackFileEC> pack(pas::construct_call<EC_HsFile::TPackFileEC>(EC_HsFile::TPackFileEC_Create));
    pack->SetPackagePath(static_cast<pas::AnsiString>(list->GetParamValue(i)));
    EC_HsFile::PackageCollection->AddPackToBack(pack.get());pack.release();
   }
   EC_HsFile::PackageCollection->CloseAllPackages();assert(EC_HsFile::PackageCollection->OpenAllPackages());
  }catch(...){aPacket::FinalizePackageCollection();throw;}
 }
 ~Packages(){aPacket::FinalizePackageCollection();}
};
int main(int argc,char** argv){
 if(argc!=3){std::fputs("Expected real game root and private output root\n",stderr);return 2;}
 std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 Packages packages;
 OriginalOwner<EC_Data::TDataEC> data(pas::construct_call<EC_Data::TDataEC>(EC_Data::TDataEC_Create));
 OriginalOwner<EC_BlockPar::TBlockParEC> config(pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create));
 OriginalOwner<EC_BlockPar::TBlockParEC> language(pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create));
 data->LoadFromEncryptedDatFile(u"CFG\\CacheData.dat"_w);
 config->LoadFromEncryptedDatFile(u"CFG\\Main.dat"_w);language->LoadFromEncryptedDatFile(u"CFG\\Rus\\Lang.dat"_w);
 GlobalsV::HardwareRenderingEnabled=false;GlobalsV::SoundEnabled=true;GlobalsV::MusicEnabled=true;
 GlobalsV::MusicVolume=.8f;GlobalsV::MusicVolumeScale=.75f;Robot::RobotMusic=true;System::RandSeed=37;
 {
  m::RangersAudioSession audio(*data);m::RangersMusicSession music(*config,*language);
  {
   m::EngineCallbacks binding(m::MakeRangersHostCallbacks());auto* table=m::EngineCallbacks::Table();
   table->m_ProgressBar(.5f);table->m_Begin();
   table->m_MusicVolumeSet(.5f);assert(table->m_MusicVolumeGet()==.5f);
   wchar_t effect[]=L"Sound.FormShipClose";
   const auto sound=table->m_SoundCreate(effect,0,0);
   table->m_SoundVolume(sound,.5f);table->m_SoundPan(sound,.25f);
   assert(table->m_SoundGetVolume(sound)==.5f && table->m_SoundGetPan(sound)==.25f);
   table->m_SoundPlay(sound);assert(table->m_SoundIsPlay(sound));table->m_SoundDestroy(sound);
   table->m_Sound(effect);
   wchar_t text[]=L"Rangers MatrixGame",font[]=L"Font.2Normal";SMGDRangersInterfaceText image{};
   table->m_RangersText(text,font,0xffffffff,200,64,0,0,0,0,0,nullptr,&image);
   assert(image.m_Buf && image.m_SizeX==200 && image.m_SizeY==64);table->m_RangersTextClear(&image);assert(!image.m_Image && !image.m_Buf);
   table->m_Takt();bool consumed=false;
   const auto start=SDL_GetTicks();
   do{
    auto* manager=GR_Main::MusicManager;pas::critical_enter(manager->ControlLock);
    auto stats=srhd_awa::platform::e2e4_game_audio::GetStreamStats(manager->Current->Buffer);
    consumed=stats.valid && stats.played_frames>=1024 && !stats.rejected_writes;
    pas::critical_leave(manager->ControlLock);if(consumed)break;SDL_Delay(20);
   }while(SDL_GetTicks()-start<30000);
   assert(consumed);
  }
 }
 assert(!GR_Main::MusicManager && !GR_Main::SoundManager && !GR_Main::GlobalCache && !GR_Main::MainDataConfig && !GR_Main::CacheDataRoot);
 std::puts("REAL_HOST_ENGINE full16=PASS text=PASS sound=PASS music_stream=PASS teardown=PASS NO_MATRIX_INIT NO_SWITCH_PROOF");
 return 0;
}
