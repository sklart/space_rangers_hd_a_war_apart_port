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
#include "rangers_data_session.hpp"
#include "ec_file_adapter.hpp"
#include "e2e4_game_audio_bridge.hpp"
#include <SDL2/SDL.h>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <memory>
namespace m=srhd_awa::matrixgame;
int main(int argc,char** argv){
 if(argc!=3){std::fputs("Expected real game root and private output root\n",stderr);return 2;}
 std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 GlobalsV::HardwareRenderingEnabled=false;GlobalsV::SoundEnabled=true;GlobalsV::MusicEnabled=true;
 GlobalsV::MusicVolume=.8f;GlobalsV::MusicVolumeScale=.75f;Robot::RobotMusic=true;System::RandSeed=37;
 {
  m::RangersDataSession data;auto* main_config=&data.MainConfig();auto* cache_data=&data.CacheData();
  // Failure in a borrowed audio owner must preserve the shared data context.
#ifndef __SWITCH__
  SDL_setenv("SDL_AUDIODRIVER","e573-invalid-driver",1);bool failed=false;
  try{m::RangersAudioSession rejected(data);}catch(const std::runtime_error&){failed=true;}
  assert(failed && !GR_Main::GlobalCache && !GR_Main::SoundManager && GR_Main::CacheDataRoot==cache_data && GR_Main::MainDataConfig==main_config && GR_Main::WideCaseTable.length());
  SDL_setenv("SDL_AUDIODRIVER","dummy",1);
#endif
  {
  m::RangersAudioSession audio(data);m::RangersMusicSession music(data);
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
  assert(GR_Main::MainDataConfig==main_config && GR_Main::CacheDataRoot==cache_data && GR_Main::WideCaseTable.length());
 }
 assert(!GR_Main::MusicManager && !GR_Main::SoundManager && !GR_Main::GlobalCache && !GR_Main::MainDataConfig && !GR_Main::CacheDataRoot);
 std::puts("OWNED_HOST_ENGINE full16=PASS text=PASS sound=PASS music_stream=PASS teardown=PASS NO_MATRIX_INIT NO_SWITCH_PROOF");
 return 0;
}
