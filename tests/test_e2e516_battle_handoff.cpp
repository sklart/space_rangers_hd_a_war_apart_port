#include "../port/switch/platform/e2e516_battle_handoff.hpp"
#include "../port/switch/platform/sdl_presentation_handoff.hpp"
#include "../port/switch/platform/e2e516_sdl_battle_handoff.hpp"
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>
using namespace srhd_awa::platform::planet_battle;
namespace srhd_awa::platform::renderer_platform {
SdlPresentationHandoff mock_sdl;
void SuspendSdlPresentationForBattle(){mock_sdl.Suspend([]{});}
void ResumeSdlPresentationAfterBattle(){mock_sdl.Resume();}
}
template<class F>void Reject(F&& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
int main(){
 {
  srhd_awa::platform::renderer_platform::SdlPresentationHandoff sdl;
  Reject([&]{sdl.Resume();});
  Reject([&]{sdl.Suspend([]{throw std::runtime_error("release");});});
  assert(!sdl.Suspended());int releases=0;sdl.Suspend([&]{++releases;});
  assert(sdl.Suspended() && releases==1);
  Reject([&]{sdl.Suspend([&]{++releases;});});assert(releases==1);
  sdl.Resume();assert(!sdl.Suspended());
 }
 {
  using srhd_awa::platform::renderer_platform::mock_sdl;
  bool native=false;
  E2e516Handoff provider(E2e516SdlOperations(
   [&]{assert(mock_sdl.Suspended() && !native);native=true;},
   [&]{assert(mock_sdl.Suspended() && native);native=false;}));
  auto lease=Capture();lease->EnterBattle();assert(native && mock_sdl.Suspended());
  lease->Restore();assert(!native && !mock_sdl.Suspended());
 }
 Reject([]{E2e516Handoff invalid({});});assert(!active);
 std::vector<std::string> events;
 bool fail_release=false,fail_start=false,fail_stop=false,fail_resume=false;
 E2e516Handoff provider({
  [&]{events.push_back("release");if(fail_release)throw std::runtime_error("release");},
  [&]{events.push_back("start");if(fail_start)throw std::runtime_error("start");},
  [&]{events.push_back("stop");if(fail_stop)throw std::runtime_error("stop");},
  [&]{events.push_back("resume");if(fail_resume)throw std::runtime_error("resume");}
 });
 {auto lease=Capture();assert(lease->UsesNativeWindow());Reject([&]{Capture();});assert(events.empty());lease->EnterBattle();lease->Restore();lease->Restore();}
 assert((events==std::vector<std::string>{"release","start","stop","resume"}));events.clear();
 {auto lease=Capture();fail_release=true;Reject([&]{lease->EnterBattle();});fail_release=false;lease->Restore();}
 assert((events==std::vector<std::string>{"release"}));events.clear();
 {auto lease=Capture();fail_start=true;Reject([&]{lease->EnterBattle();});fail_start=false;lease->Restore();}
 assert((events==std::vector<std::string>{"release","start","resume"}));events.clear();
 {auto lease=Capture();fail_start=true;fail_resume=true;Reject([&]{lease->EnterBattle();});fail_start=false;fail_resume=false;lease->Restore();}
 assert((events==std::vector<std::string>{"release","start","resume","resume"}));events.clear();
 {auto lease=Capture();lease->EnterBattle();fail_stop=true;Reject([&]{lease->Restore();});fail_stop=false;lease->Restore();}
 assert((events==std::vector<std::string>{"release","start","stop","stop","resume"}));events.clear();
 {auto lease=Capture();lease->EnterBattle();fail_resume=true;Reject([&]{lease->Restore();});fail_resume=false;lease->Restore();}
 assert((events==std::vector<std::string>{"release","start","stop","resume","resume"}));events.clear();
 {auto lease=Capture();lease->EnterBattle();}
 assert((events==std::vector<std::string>{"release","start","stop","resume"}));
}
