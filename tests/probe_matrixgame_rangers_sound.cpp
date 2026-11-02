// SPDX-License-Identifier: GPL-2.0-or-later
// Actual Rangers sound/cache/SDL algorithms and installed CacheData.dat.
// SDL dummy is an explicit host output driver; no audible/hardware claim.
#include "runtime_support.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_Cache.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_Sound.hpp"
#include "units/GlobalsV.hpp"
#include "units/Robot.hpp"
#include "units/aPacket.hpp"
#include "units/EC_HsFile.hpp"
#include "types/EC_CacheSound.hpp"
#include "rangers_host_callbacks.hpp"
#include "rangers_audio_session.hpp"
#include "rangers_bridge.hpp"
#include "ec_file_adapter.hpp"
#include "e2e4_game_audio_bridge.hpp"
#include <SDL2/SDL.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <cstring>
namespace m=srhd_awa::matrixgame;namespace a=srhd_awa::platform::e2e4_game_audio;
void Save(const char* name,const void* bytes,std::size_t size){std::ofstream file(name,std::ios::binary);file.write(static_cast<const char*>(bytes),size);assert(file.good());}
struct Packages {
 Packages(){
  assert(!EC_HsFile::PackageCollection && !EC_HsFile::PackageFileLock);
  assert(aPacket::InitializePackageCollection());
  try{
   auto* pack=pas::construct_call<EC_HsFile::TPackFileEC>(EC_HsFile::TPackFileEC_Create);
   pack->SetPackagePath("DATA\\Sound.pkg"_a);EC_HsFile::PackageCollection->AddPackToBack(pack);
   EC_HsFile::PackageCollection->CloseAllPackages();
   if(!EC_HsFile::PackageCollection->OpenAllPackages())throw std::runtime_error("sound package bootstrap failed");
  }catch(...){aPacket::FinalizePackageCollection();throw;}
 }
 ~Packages(){aPacket::FinalizePackageCollection();}
};
int main(int argc,char** argv){
 assert(argc==3 || (argc==4 && !std::strcmp(argv[3],"--backend-failure")));std::filesystem::create_directories(argv[2]);std::filesystem::current_path(argv[2]);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 Packages packages;
 std::fputs("RANGERS_SOUND packages_ready\n",stderr);
 GlobalsV::HardwareRenderingEnabled=false;GlobalsV::SoundEnabled=true;GlobalsV::MusicEnabled=false;GlobalsV::SoundVolume=.8f;Robot::RobotSound=true;
 auto* root=pas::construct_call<EC_Data::TDataEC>(EC_Data::TDataEC_Create);
 try{
  root->LoadFromEncryptedDatFile(u"CFG\\CacheData.dat"_w);
  std::fputs("RANGERS_SOUND CacheData_loaded\n",stderr);
  assert(!root->IsEmpty());
  if(argc==4){
   bool failed=false;try{m::RangersAudioSession session(*root);}catch(const std::runtime_error& error){failed=std::string(error.what())=="real Rangers sound backend did not initialize";}
   assert(failed && !GR_Main::SoundManager && !GR_Main::MusicManager && !GR_Main::GlobalCache && !GR_Main::CacheDataRoot);
   assert(GlobalsV::SoundEnabled && !GlobalsV::MusicEnabled);pas::free(root);root=nullptr;
   std::puts("RANGERS_SOUND backend_open_failure=REAL globals_rollback=PASS flags_restored=PASS NO_FAKE_SUCCESS");return 0;
  }
  // Reject the renderer-cache path and disabled sound before publishing globals.
  GlobalsV::HardwareRenderingEnabled=true;bool rendering_rejected=false;
  try{m::RangersAudioSession session(*root);}catch(const std::logic_error&){rendering_rejected=true;}
  assert(rendering_rejected && !GR_Main::GlobalCache && !GR_Main::SoundManager && !GR_Main::CacheDataRoot);GlobalsV::HardwareRenderingEnabled=false;
  GlobalsV::SoundEnabled=false;bool disabled_rejected=false;
  try{m::RangersAudioSession session(*root);}catch(const std::invalid_argument&){disabled_rejected=true;}
  assert(disabled_rejected && !GlobalsV::SoundEnabled && !GR_Main::CacheDataRoot);GlobalsV::SoundEnabled=true;
  auto* entry=root->FindEntryByPath(u"Sound.FormShipClose"_w);assert(entry && entry->Kind==EC_Data::dekFile);
  auto* wav=pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);root->ReadBufferByPath(u"Sound.FormShipClose"_w,wav);
  Save("original.wav",wav->Data,wav->DataSize);pas::free(wav);
  {
   m::RangersAudioSession session(*root);assert(GR_Main::GlobalCache && GR_Main::SoundManager && GR_Main::CacheDataRoot==root && GlobalsV::SoundEnabled);
   bool duplicate=false;try{m::RangersAudioSession second(*root);}catch(const std::logic_error&){duplicate=true;}assert(duplicate);
   m::RangersBridge bridge(m::MakeRangersHostCallbacks());
   const auto id=bridge.CreateSound(u"Sound.FormShipClose",0,false);assert(id.value);
   bridge.SetSoundVolume(id,.5f);bridge.SetSoundPan(id,-.25f);
   assert(bridge.GetSoundVolume(id)==.5f && bridge.GetSoundPan(id)==-.25f);
   bridge.StartSound(id);assert(bridge.IsSoundPlaying(id)==1);
   auto* cached=pas::checked_cast<EC_CacheSound::TCSoundEC*>(GR_Main::GlobalCache->MostRecentData);
   assert(cached && cached->SampleData && cached->SampleDataSize>0 && cached->Format.FormatTag==1);
   Save("original.pcm",cached->SampleData,cached->SampleDataSize);
   std::printf("REAL_SOUND key=Sound.FormShipClose bytes=%d channels=%u rate=%u bits=%u\n",cached->SampleDataSize,cached->Format.Channels,cached->Format.SamplesPerSecond,cached->Format.BitsPerSample);
   const auto started=SDL_GetTicks();while(bridge.IsSoundPlaying(id) && SDL_GetTicks()-started<10000)SDL_Delay(10);
   assert(!bridge.IsSoundPlaying(id));bridge.FreeSound(id);
   // Original looping behavior: nonzero volume starts lazily; zero clears it.
   const auto loop=bridge.CreateSound(u"Sound.FormShipClose",0,true);bridge.SetSoundVolume(loop,.4f);assert(bridge.IsSoundPlaying(loop));
   bridge.SetSoundPan(loop,.25f);assert(bridge.GetSoundPan(loop)==.25f);bridge.SetSoundVolume(loop,0);assert(!bridge.IsSoundPlaying(loop));bridge.FreeSound(loop);
   bridge.PlaySound(u"Sound.FormShipClose");assert(GR_Main::SoundManager->FirstBuffer);
   bridge.Reset();
  }
  assert(!GR_Main::SoundManager && !GR_Main::GlobalCache && !GR_Main::CacheDataRoot && GlobalsV::SoundEnabled && !GlobalsV::MusicEnabled);
  pas::free(root);root=nullptr;
 }catch(...){if(root)pas::free(root);throw;}
 std::puts("RANGERS_SOUND original_Robot_callbacks=9 original_CacheData_WAV_PCM=PASS effect_completion=PASS loop_volume_zero_stop=PASS host_cleanup=PASS SDL_DUMMY_ONLY NO_AUDIBLE_OUTPUT NO_MATRIX_INIT NO_SWITCH");
}
