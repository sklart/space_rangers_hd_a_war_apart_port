// SPDX-License-Identifier: GPL-2.0-or-later
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_HsFile.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_Music.hpp"
#include "units/GR_Sound.hpp"
#include "units/GlobalsV.hpp"
#include "units/Robot.hpp"
#include "rangers_host_session.hpp"
#include "engine_callbacks.hpp"
#include "ec_file_adapter.hpp"
#include "e2e4_game_audio_bridge.hpp"
#include <SDL2/SDL.h>
#include <cassert>
#include <cstdio>
#include <filesystem>
namespace m=srhd_awa::matrixgame;
struct Settings {
 std::uint8_t hardware=GlobalsV::HardwareRenderingEnabled,sound=GlobalsV::SoundEnabled,music=GlobalsV::MusicEnabled,robot_sound=Robot::RobotSound,robot_music=Robot::RobotMusic;
 float scale=GlobalsV::MusicVolumeScale;
 void Restored()const{assert(GlobalsV::HardwareRenderingEnabled==hardware && GlobalsV::SoundEnabled==sound && GlobalsV::MusicEnabled==music && Robot::RobotSound==robot_sound && Robot::RobotMusic==robot_music && GlobalsV::MusicVolumeScale==scale);}
};
void Clean(){assert(!m::EngineCallbacks::Bound() && !GR_Main::MainDataConfig && !GR_Main::LanguageDataConfig && !GR_Main::CacheDataRoot && !GR_Main::SoundManager && !GR_Main::MusicManager && !GR_Main::GlobalCache && !GR_Main::WideCaseTable.length() && !EC_HsFile::PackageCollection && !EC_HsFile::PackageFileLock && !SDL_WasInit(SDL_INIT_AUDIO));}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
int main(int argc,char** argv){
 assert(argc==3);std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);Clean();
 const Settings saved;
 for(bool effects:{false,true})for(bool music:{false,true}){
  {
   m::RangersHostOptions options;options.sound=effects;options.music=music;m::RangersHostSession host(options);
   assert(m::EngineCallbacks::Bound() && GR_Main::MainDataConfig && GR_Main::CacheDataRoot && GR_Main::GlobalCache && GR_Main::SoundManager);
   assert(!GlobalsV::HardwareRenderingEnabled && GlobalsV::SoundEnabled==effects && GlobalsV::MusicEnabled==music && bool(Robot::RobotSound)==effects && bool(Robot::RobotMusic)==music);
   assert(bool(GR_Main::MusicManager)==music && bool(SDL_WasInit(SDL_INIT_AUDIO))==(effects||music));
   auto* root=GR_Main::MainDataConfig;Reject([&]{m::RangersHostSession duplicate;});assert(GR_Main::MainDataConfig==root);
   const auto& table=host.Table();table.m_ProgressBar(.5f);table.m_Begin();
   table.m_MusicVolumeSet(.5f);assert(table.m_MusicVolumeGet()==.5f);
   wchar_t effect[]=L"Sound.FormShipClose";const auto sound=table.m_SoundCreate(effect,0,0);assert(sound);
   table.m_SoundVolume(sound,.5f);table.m_SoundPan(sound,.25f);
   assert(table.m_SoundGetVolume(sound)==.5f && table.m_SoundGetPan(sound)==.25f);
   table.m_SoundPlay(sound);assert(bool(table.m_SoundIsPlay(sound))==effects);table.m_SoundDestroy(sound);table.m_Sound(effect);
   wchar_t text[]=L"Rangers host",font[]=L"Font.2Normal";SMGDRangersInterfaceText image{};
   table.m_RangersText(text,font,0xffffffff,200,64,0,0,0,0,0,nullptr,&image);assert(image.m_Buf);table.m_RangersTextClear(&image);
   table.m_Takt();
   if(music){
    bool consumed=false;const auto start=SDL_GetTicks();
    do{auto* manager=GR_Main::MusicManager;pas::critical_enter(manager->ControlLock);
     const auto stats=srhd_awa::platform::e2e4_game_audio::GetStreamStats(manager->Current->Buffer);
     consumed=stats.valid && stats.played_frames>=1024 && !stats.rejected_writes;
     pas::critical_leave(manager->ControlLock);if(consumed)break;SDL_Delay(20);
    }while(SDL_GetTicks()-start<30000);assert(consumed);
   }else assert(!GR_Main::MusicManager);
   if(!effects)assert(!GR_Main::SoundManager->FirstBuffer || music);
  }
  saved.Restored();Clean();std::printf("HOST_SESSION effects=%d music=%d full16=PASS teardown=PASS\n",effects,music);std::fflush(stdout);
 }
 // Original fully muted mode must not try to open any backend, even an invalid one.
 SDL_setenv("SDL_AUDIODRIVER","e574-invalid-driver",1);
 {m::RangersHostOptions muted;muted.sound=false;muted.music=false;m::RangersHostSession host(muted);assert(!SDL_WasInit(SDL_INIT_AUDIO));}
 saved.Restored();Clean();Reject([]{m::RangersHostSession failing;});saved.Restored();Clean();
 SDL_setenv("SDL_AUDIODRIVER","dummy",1);{m::RangersHostSession recovery;assert(m::EngineCallbacks::Bound());}saved.Restored();Clean();
 std::puts("HOST_SESSION modes=4 original_muted_control=PASS failure_rollback=PASS retry=PASS flags_restore=PASS SDL_DUMMY_ONLY NO_NATIVE_INIT NO_SWITCH");return 0;
}
