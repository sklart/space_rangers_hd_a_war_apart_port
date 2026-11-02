// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#error Galaxy prerequisite evidence requires assertions
#endif
#include "rangers_galaxy_prerequisites.hpp"
#include "rangers_script_session.hpp"
#include "rangers_host_session.hpp"
#include "ec_file_adapter.hpp"
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/aGalaxy.hpp"
#include "units/aPlayer.hpp"
#include "units/fHangar.hpp"
#include "types/GI_GraphBuf.hpp"
#include "types/GI_GraphButton.hpp"
#include "types/aMyFunction.hpp"
#include <cassert>
#include <cstdio>
#include <memory>
#include <stdexcept>
namespace m=srhd_awa::matrixgame;
namespace {
template<class T>struct OriginalDelete{void operator()(T* p)const{pas::free(p);}};
template<class T>using Owner=std::unique_ptr<T,OriginalDelete<T>>;
template<class F>void Reject(F action){bool rejected=false;try{action();}catch(...){rejected=true;}assert(rejected);}
void Empty(){assert(!Globals::HangarScreen && !GlobalsV::RegisteredScreens[GlobalsV::screenHangar] && !GR_Main::CCInterface && !aGalaxy::Galaxy && !aGalaxy::PlayerStar && !aPlayer::GetPlayer());}
struct Pixels {
 Owner<GR_GraphBuf::TPixelFormatGR> value{pas::make_object<GR_GraphBuf::TPixelFormatGR>()};
 Pixels(){assert(!GR_Main::CurrentPixelFormat);value->RedMask=0xf800;value->GreenMask=0x07e0;value->BlueMask=0x001f;value->AlphaMask=0;value->BytesPerPixel=2;value->RebuildChannelMetrics();GR_Main::CurrentPixelFormat=value.get();}
 ~Pixels(){assert(GR_Main::CurrentPixelFormat==value.get());GR_Main::CurrentPixelFormat=nullptr;}
};
std::size_t Inspect(m::RangersGalaxyPrerequisites& prerequisite){
 auto& hangar=prerequisite.Hangar();assert(Globals::HangarScreen==&hangar && GlobalsV::RegisteredScreens[GlobalsV::screenHangar]==&hangar);
 assert(hangar.RootUiObject && hangar.MainPanel && hangar.PlanetPanel && hangar.StationPanel && hangar.LoadPanel && hangar.ShipInfoWindow);
 std::size_t count=0;const auto visit=[&](auto&& self,GI_MessageLoop::TObjectGI* p)->void{for(;p;p=p->NextSibling){assert(++count<100000 && p->MessageLoop==&hangar);if(p->FirstChild)self(self,p->FirstChild);}};
 visit(visit,hangar.RootUiObject);assert(count>100);
 assert(pas::assigned(pas::checked_cast<GI_GraphButton::TGraphButtonGI*>(hangar.GetByName(u"ButRepair"sv))->UpCallback));
 for(int i=0;i<9;++i)assert(hangar.ShipSlots[i].ImageBuffer && hangar.ShipSlots[i].ImageControl && hangar.ShipSlots[i].ImageControl->MessageLoop==&hangar);
 assert(GR_Main::CCInterface && GR_Main::CCInterface->Buffer && GR_Main::CCInterface->Lock && GR_Main::CCInterface->SnapshotHead);
 assert(GR_Main::CCInterface->GetSnapshot());return count;
}
}
int main(int argc,char** argv){
 assert(argc==3);std::setvbuf(stdout,nullptr,_IONBF,0);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 Empty();Reject([]{m::RangersGalaxyPrerequisites no_host;});Empty();
 {
  m::RangersHostOptions options;options.sound=false;options.music=false;m::RangersHostSession host(options);
  Reject([]{m::RangersGalaxyPrerequisites no_scripts;});Empty();
  m::RangersScriptSession scripts;Reject([]{m::RangersGalaxyPrerequisites no_pixels;});Empty();Pixels pixels;
  GR_Main::GameScreenWidth=1024;GR_Main::GameScreenHeight=768;GR_Main::GameScreenRect={0,0,1024,768};GR_Main::ExtraScreenWidth=0;GR_Main::ExtraScreenHeight=0;
  auto* main=GR_Main::MainDataConfig;
  Owner<EC_BlockPar::TBlockParEC> clone{pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create)};
  clone->CopyFrom(main);clone->GetBlockByPath(u"ML.Hangar.Panel"_wref.get())->Clear();
  GR_Main::MainDataConfig=clone.get();Reject([]{m::RangersGalaxyPrerequisites malformed;});GR_Main::MainDataConfig=main;
  Empty();assert(!GR_Main::UiStyleConfig && !GR_Main::GameDataConfig && !GR_Main::UiDepthConfig);
  {
   Owner<GR_Main::TCCInterface> foreign{pas::construct_call<GR_Main::TCCInterface>(GR_Main::TCCInterface_Create)};
   GR_Main::CCInterface=foreign.get();auto* head=foreign->SnapshotHead;
   Reject([]{m::RangersGalaxyPrerequisites conflict;});assert(GR_Main::CCInterface==foreign.get() && foreign->SnapshotHead==head);GR_Main::CCInterface=nullptr;
  }
  auto* style=main->GetBlockByPath(u"ML"_wref.get());auto* data=main->GetBlockByPath(u"Data"_wref.get());auto* depth=main->GetBlockByPath(u"ZPos"_wref.get());
  GR_Main::UiDepthConfig=clone.get();Reject([]{m::RangersGalaxyPrerequisites foreign;});assert(GR_Main::UiDepthConfig==clone.get());GR_Main::UiDepthConfig=nullptr;
  for(int cycle=0;cycle<2;++cycle){
   if(cycle){GR_Main::UiStyleConfig=style;GR_Main::GameDataConfig=data;GR_Main::UiDepthConfig=depth;}
   {
    m::RangersGalaxyPrerequisites prerequisite;const auto controls=Inspect(prerequisite);
    Reject([]{m::RangersGalaxyPrerequisites duplicate;});assert(Inspect(prerequisite)==controls);
    assert(!GlobalsV::MemorySnapshotActive);
    {
     Owner<aGalaxy::TGalaxy> world{pas::construct_call<aGalaxy::TGalaxy>(aGalaxy::TGalaxy_Create)};aGalaxy::Galaxy=world.get();
     assert(world->Stars && world->Planets && world->Constellations && world->Rangers && world->Scripts && world->GalaxyEvents);
     assert(pas::list_count(world->Stars)==0 && pas::list_count(world->Rangers)==0 && !aPlayer::GetPlayer());
     for(int i=0;i<9;++i)assert(prerequisite.Hangar().ShipSlots[i].ShipId==0);
     Reject([]{m::RangersGalaxyPrerequisites live_world;});assert(Inspect(prerequisite)==controls);
     world.reset();aGalaxy::Galaxy=nullptr;
    }
    assert(!GlobalsV::MemorySnapshotActive && !aPlayer::GetPlayer() && !aGalaxy::PlayerStar);
    assert(Inspect(prerequisite)==controls);
    std::printf("GALAXY_PREREQUISITES cycle=%d controls=%zu original_Hangar_config_layout=PASS original_TCCInterface=PASS original_TGalaxy_create_destroy=PASS integrity_operations=UNMODIFIED campaign_Player=NOT_GENERATED\n",cycle,controls);
   }
   Empty();assert(GR_Main::UiStyleConfig==(cycle?style:nullptr) && GR_Main::GameDataConfig==(cycle?data:nullptr) && GR_Main::UiDepthConfig==(cycle?depth:nullptr));
  }
  GR_Main::UiStyleConfig=nullptr;GR_Main::GameDataConfig=nullptr;GR_Main::UiDepthConfig=nullptr;
 }
 Empty();assert(!GR_Main::MainDataConfig && !GR_Main::GlobalCache && !GR_Main::CurrentPixelFormat);
 std::puts("GALAXY_PREREQUISITES partial_layout_rollback=PASS foreign_TCC_roots_duplicate=REJECTED original_cleanup_borrowed_roots=PASS no_save_load=PASS campaign_generation=NOT_RUN");
}
