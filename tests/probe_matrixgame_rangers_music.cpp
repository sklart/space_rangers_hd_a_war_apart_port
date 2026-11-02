// SPDX-License-Identifier: GPL-2.0-or-later
// Actual Robot music callbacks, actual config/Ogg/worker/SDL path; dummy device.
#include "runtime_support.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Str.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_Music.hpp"
#include "units/GR_Sound.hpp"
#include "units/GR_DX.hpp"
#include "units/GlobalsV.hpp"
#include "units/Robot.hpp"
#include "units/aPacket.hpp"
#include "units/EC_HsFile.hpp"
#include "units/System.hpp"
#include "rangers_host_callbacks.hpp"
#include "rangers_audio_session.hpp"
#include "rangers_music_session.hpp"
#include "rangers_bridge.hpp"
#include "ec_file_adapter.hpp"
#include "e2e4_game_audio_bridge.hpp"
#include <SDL2/SDL.h>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <cstring>
namespace m=srhd_awa::matrixgame;namespace a=srhd_awa::platform::e2e4_game_audio;
struct Packages {
 Packages(){
  assert(!EC_HsFile::PackageCollection);assert(aPacket::InitializePackageCollection());
  auto* install=pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  try{
   install->LoadFromTextFileWithEncodingProbe(pas::literal_pointer(u"INSTALL.TXT"),false);auto* list=install->GetBlock(u"Packages"sv);
   for(int i=0;i<list->GetParamCount();++i){auto* pack=pas::construct_call<EC_HsFile::TPackFileEC>(EC_HsFile::TPackFileEC_Create);pack->SetPackagePath(static_cast<pas::AnsiString>(list->GetParamValue(i)));EC_HsFile::PackageCollection->AddPackToBack(pack);}
   pas::free(install);install=nullptr;EC_HsFile::PackageCollection->CloseAllPackages();assert(EC_HsFile::PackageCollection->OpenAllPackages());
  }catch(...){if(install)pas::free(install);aPacket::FinalizePackageCollection();throw;}
 }
 ~Packages(){aPacket::FinalizePackageCollection();}
};
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
int main(int argc,char** argv){
 assert(argc==3 || (argc==4 && std::strcmp(argv[3],"--music-only")==0));
 const bool music_only=argc==4;
 std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);Packages packages;
 GlobalsV::HardwareRenderingEnabled=false;GlobalsV::SoundEnabled=!music_only;GlobalsV::MusicEnabled=true;
 GlobalsV::MusicVolume=.8f;GlobalsV::MusicVolumeScale=.75f;Robot::RobotMusic=true;System::RandSeed=37;
 auto* data=pas::construct_call<EC_Data::TDataEC>(EC_Data::TDataEC_Create);
 auto* config=pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
 auto* language=pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
 try{
  data->LoadFromEncryptedDatFile(u"CFG\\CacheData.dat"_w);config->LoadFromEncryptedDatFile(u"CFG\\Main.dat"_w);
  language->LoadFromEncryptedDatFile(u"CFG\\Rus\\Lang.dat"_w);
  Reject([&]{m::RangersMusicSession missing_audio(*config,*language);});assert(!GR_Main::MainDataConfig && !GR_Main::MusicManager);
  {
   m::RangersAudioSession audio(*data);
   assert(GlobalsV::SoundEnabled==!music_only && GlobalsV::MusicEnabled);
   GlobalsV::MusicEnabled=false;Reject([&]{m::RangersMusicSession disabled(*config,*language);});assert(!GR_Main::MusicManager && !GR_Main::MainDataConfig);GlobalsV::MusicEnabled=true;
   GR_Main::WideCaseTable.set_length(1);Reject([&]{m::RangersMusicSession foreign_case(*config,*language);});assert(GR_Main::WideCaseTable.length()==1);GR_Main::WideCaseTable=nullptr;
   {
    m::RangersMusicSession music(*config,*language);auto* manager=GR_Main::MusicManager;assert(manager && GR_Main::MainDataConfig==config);
    assert(EC_Str::LowerCaseWideString(u"Music"_w)==u"music");
    Reject([&]{m::RangersMusicSession duplicate(*config,*language);});assert(GR_Main::MusicManager==manager);
    m::RangersBridge bridge(m::MakeRangersHostCallbacks());
    assert(GR_Main::Direct3DDevice==nullptr && !GR_DX::TextureCaches);
    bridge.SetProgress(.5f);bridge.ReleaseTextures();assert(GR_Main::Direct3DDevice==nullptr && !GR_DX::TextureCaches);
    assert(bridge.GetMusicVolume()==.75f);bridge.SetMusicVolume(.5f);assert(bridge.GetMusicVolume()==.5f);
    bridge.PlayMusic();
    a::StreamStats stats{};pas::WideString selected{};
    const auto start=SDL_GetTicks();
    do{
     pas::critical_enter(manager->ControlLock);
     stats=a::GetStreamStats(manager->Current->Buffer);selected=manager->Current->GetFileName();
     pas::critical_leave(manager->ControlLock);
     if(stats.valid && stats.played_frames>=1024)break;
     SDL_Delay(20);
    }while(SDL_GetTicks()-start<30000);
    assert(stats.valid && stats.played_frames>=1024 && stats.callback_count>0 && stats.rejected_writes==0 && selected!=u"");
    auto* playlist=config->GetBlockByPath(u"Music.Robot"_w);bool listed=false;
    for(int i=0;i<playlist->GetParamCount();++i)if(EC_Str::TrimWideString(EC_Str::LowerCaseWideString(playlist->GetParamValue(i)))==selected)listed=true;
    assert(listed && selected==EC_Str::LowerCaseWideString(selected));
    auto name=static_cast<pas::AnsiString>(selected);
    std::printf("REAL_MUSIC selected=%s played_frames=%llu callbacks=%llu ready=%u rejected=%llu\n",name.c_str(),static_cast<unsigned long long>(stats.played_frames),static_cast<unsigned long long>(stats.callback_count),stats.ready_chunks,static_cast<unsigned long long>(stats.rejected_writes));std::fflush(stdout);
    bridge.SetMusicVolume(.25f);assert(bridge.GetMusicVolume()==.25f);
    pas::critical_enter(manager->ControlLock);assert(manager->Current->Buffer->Volume==GlobalsV::MusicVolume*.25f);pas::critical_leave(manager->ControlLock);
    bridge.PlayMusic(); // Original guard keeps selected music instead of restarting.
    pas::critical_enter(manager->ControlLock);assert(manager->Current->GetFileName()==selected);pas::critical_leave(manager->ControlLock);
   }
   assert(!GR_Main::MusicManager && !GR_Main::MainDataConfig && !GR_Main::WideCaseTable.length() && GR_Main::SoundManager && !GR_Main::SoundManager->FirstBuffer);
  }
  assert(!GR_Main::SoundManager && !GR_Main::GlobalCache && !GR_Main::CacheDataRoot && !SDL_WasInit(SDL_INIT_AUDIO));
  assert(GlobalsV::SoundEnabled==!music_only && GlobalsV::MusicEnabled);
  pas::free(language);language=nullptr;
  pas::free(config);config=nullptr;pas::free(data);data=nullptr;
 }catch(...){if(language)pas::free(language);if(config)pas::free(config);if(data)pas::free(data);throw;}
 std::printf("MODE sound_enabled=%d music_enabled=1 case_normalization=PASS playlist_membership=PASS\n",!music_only);
 std::puts("RANGERS_MUSIC actual_Robot_callbacks=3 actual_Music_Robot_playlist=PASS real_Ogg_stream_consumed=PASS worker_teardown=PASS preflight=PASS duplicate=PASS original_progress_no_main_device=PASS original_release_empty_textures=PASS SDL_DUMMY_ONLY NO_AUDIBLE_OUTPUT NO_GUI NO_MATRIX_INIT NO_SWITCH");
}
