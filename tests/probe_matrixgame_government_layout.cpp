// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#error Government lifecycle evidence requires assertions
#endif
#include "rangers_government_session.hpp"
#include "rangers_host_session.hpp"
#include "engine_callbacks.hpp"
#include "ec_file_adapter.hpp"
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/fGov.hpp"
#include "types/GI_GraphButton.hpp"
#include "units/aGalaxy.hpp"
#include "units/aPlayer.hpp"
#include <cassert>
#include <cstdio>
#include <memory>
#include <stdexcept>
namespace m=srhd_awa::matrixgame;
namespace {
template<class T>struct OriginalDelete {void operator()(T* p)const{pas::free(p);}};
template<class T>using Owner=std::unique_ptr<T,OriginalDelete<T>>;
void Unbound(){assert(!Globals::GovernmentScreen && !GlobalsV::RegisteredScreens[GlobalsV::screenGovernment]);}
template<class F>void Reject(F action){bool rejected=false;try{action();}catch(...){rejected=true;}assert(rejected);}
struct PixelOwner {
 Owner<GR_GraphBuf::TPixelFormatGR> pixel{pas::make_object<GR_GraphBuf::TPixelFormatGR>()};
 PixelOwner(){assert(!GR_Main::CurrentPixelFormat);pixel->RedMask=0xf800;pixel->GreenMask=0x07e0;pixel->BlueMask=0x001f;pixel->AlphaMask=0;pixel->BytesPerPixel=2;pixel->RebuildChannelMetrics();GR_Main::CurrentPixelFormat=pixel.get();}
 ~PixelOwner(){assert(GR_Main::CurrentPixelFormat==pixel.get());GR_Main::CurrentPixelFormat=nullptr;}
};
std::size_t Inspect(fGov::TfGov& gov){
 assert(Globals::GovernmentScreen==&gov && GlobalsV::RegisteredScreens[GlobalsV::screenGovernment]==&gov);
 assert(gov.RootUiObject && gov.CursorControl && gov.MainPanel && gov.PlanetPanel && gov.LoadPanel);
 assert(gov.RootUiObject->MessageLoop==&gov && gov.ContentPanel->MessageLoop==&gov);
 assert(pas::assigned(gov.GetByName(u"MainPanel"sv)->KeyDownCallback));
 assert(pas::assigned(pas::checked_cast<GI_GraphButton::TGraphButtonGI*>(gov.GetByName(u"UserMsgAdd"sv))->UpCallback));
 assert(gov.GetByName(u"PM_EndTurn"sv) && gov.GetByName(u"PM_Ship"sv) && gov.GetByName(u"TalkPA"sv) && gov.GetByName(u"PP_Gov"sv));
 std::size_t count=0;const auto visit=[&](auto&& self,GI_MessageLoop::TObjectGI* p)->void{for(;p;p=p->NextSibling){assert(++count<100000);assert(p->MessageLoop==&gov);if(p->FirstChild)self(self,p->FirstChild);}};
 visit(visit,gov.RootUiObject);assert(count>100);return count;
}
}
int main(int argc,char** argv){
 assert(argc==3);std::setvbuf(stdout,nullptr,_IONBF,0);
 srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 Unbound();Reject([]{m::RangersGovernmentSession no_host;});Unbound();
 {
  m::RangersHostOptions options;options.sound=false;options.music=false;m::RangersHostSession host(options);
  assert(m::EngineCallbacks::Bound() && GR_Main::Direct3DDevice==nullptr && !GR_Main::CurrentPixelFormat);
  assert(!GR_Main::UiStyleConfig && !GR_Main::GameDataConfig && !GR_Main::UiDepthConfig);
  Reject([]{m::RangersGovernmentSession no_pixels;});Unbound();
  PixelOwner pixels;GR_Main::GameScreenWidth=1024;GR_Main::GameScreenHeight=768;GR_Main::GameScreenRect={0,0,1024,768};
  GR_Main::ExtraScreenWidth=0;GR_Main::ExtraScreenHeight=0;
  auto* main=GR_Main::MainDataConfig;
  Owner<EC_BlockPar::TBlockParEC> clone{pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create)};
  clone->CopyFrom(main);clone->GetBlockByPath(u"ML.Gov.Panel"_wref.get())->Clear();
  GR_Main::MainDataConfig=clone.get();Reject([]{m::RangersGovernmentSession malformed;});GR_Main::MainDataConfig=main;
  Unbound();assert(!GR_Main::UiStyleConfig && !GR_Main::GameDataConfig && !GR_Main::UiDepthConfig);
  std::puts("GOVERNMENT_LAYOUT missing_host_pixels=REJECTED original_partial_tree_failure_rollback=PASS");
  auto* style=main->GetBlockByPath(u"ML"_wref.get());auto* data=main->GetBlockByPath(u"Data"_wref.get());auto* depth=main->GetBlockByPath(u"ZPos"_wref.get());
  GR_Main::UiDepthConfig=clone.get();Reject([]{m::RangersGovernmentSession foreign;});assert(GR_Main::UiDepthConfig==clone.get());GR_Main::UiDepthConfig=nullptr;Unbound();
  for(int cycle=0;cycle<2;++cycle){
   if(cycle){GR_Main::UiStyleConfig=style;GR_Main::GameDataConfig=data;GR_Main::UiDepthConfig=depth;}
   {
    m::RangersGovernmentSession government;const auto count=Inspect(government.Screen());
    Reject([]{m::RangersGovernmentSession duplicate;});assert(Inspect(government.Screen())==count);
    assert(!aGalaxy::Galaxy && !aPlayer::GetPlayer());
    std::printf("GOVERNMENT_LAYOUT cycle=%d controls=%zu original_Gov_constructor_config_layout=PASS real_callbacks=PASS duplicate_owner=REJECTED OnOpen_Galaxy=NOT_RUN\n",cycle,count);
   }
   Unbound();assert(GR_Main::UiStyleConfig==(cycle?style:nullptr) && GR_Main::GameDataConfig==(cycle?data:nullptr) && GR_Main::UiDepthConfig==(cycle?depth:nullptr));
   GR_Main::UiStyleConfig=nullptr;GR_Main::GameDataConfig=nullptr;GR_Main::UiDepthConfig=nullptr;
  }
  assert(m::EngineCallbacks::Bound() && GR_Main::MainDataConfig==main);
 }
 Unbound();assert(!m::EngineCallbacks::Bound() && !GR_Main::MainDataConfig && !GR_Main::CacheDataRoot && !GR_Main::GlobalCache && !GR_Main::CurrentPixelFormat);
 std::puts("GOVERNMENT_LAYOUT original_destructors_root_restore_host_cleanup=PASS borrowed_root_restore=PASS foreign_roots=REJECTED no_save_load=PASS CPU_LAYOUT_ONLY");
}
