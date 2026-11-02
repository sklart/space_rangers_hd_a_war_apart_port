#ifdef NDEBUG
#error Galaxy context verification requires assertions
#endif
#include "rangers_host_session.hpp"
#include "engine_callbacks.hpp"
#include "ec_file_adapter.hpp"
#include "units/GR_Main.hpp"
#include "units/GlobalsV.hpp"
#include "units/Robot.hpp"
#include "units/aGalaxy.hpp"
#include "units/aPlayer.hpp"
#include <cassert>
#include <cstdio>
int main(int argc,char** argv){
 assert(argc==3);srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 {
  srhd_awa::matrixgame::RangersHostSession host;
  const bool galaxy=aGalaxy::Galaxy!=nullptr,player=aPlayer::GetPlayer()!=nullptr;
  const bool screen=GlobalsV::RegisteredScreens[GlobalsV::screenGovernment]!=nullptr;
  const bool interface_bound=Robot::RobotInterface!=nullptr,device=GR_Main::Direct3DDevice!=nullptr;
  std::printf("GALAXY_RETURN_CONTEXT galaxy=%u player=%u government_screen=%u Robot_Run_interface=%u Rangers_D3D_device=%u callbacks=%u\n",unsigned(galaxy),unsigned(player),unsigned(screen),unsigned(interface_bound),unsigned(device),unsigned(srhd_awa::matrixgame::EngineCallbacks::Bound()));
  assert(!galaxy && !player && !screen && !interface_bound && !device);
  assert(srhd_awa::matrixgame::EngineCallbacks::Bound());
 }
 assert(!srhd_awa::matrixgame::EngineCallbacks::Bound() && !GR_Main::MainDataConfig && !GR_Main::CacheDataRoot && !GR_Main::GlobalCache && !GR_Main::SoundManager && !GR_Main::MusicManager);
 std::puts("GALAXY_RETURN_CONTEXT standalone_host_is_not_galaxy_session=CONFIRMED cleanup=PASS no_save_load=PASS");
}
