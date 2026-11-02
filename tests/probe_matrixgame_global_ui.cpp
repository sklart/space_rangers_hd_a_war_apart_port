// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#error Global UI evidence requires assertions
#endif
#include "rangers_global_ui_session.hpp"
#include "rangers_script_session.hpp"
#include "rangers_host_session.hpp"
#include "ec_file_adapter.hpp"
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Expression.hpp"
#include "units/EC_Str.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/aConst.hpp"
#include "units/aGalaxy.hpp"
#include "units/aPlayer.hpp"
#include "units/aSaveLoad.hpp"
#include "units/WindowsSdk.hpp"
#include "types/GI_MessageLoop.hpp"
#include "types/aEFilm.hpp"
#include "types/fLoad.hpp"
#include "types/aSaveLoad.hpp"
#include "units/fGov.hpp"
#include <cassert>
#include <cstdio>
#include <memory>
#include <stdexcept>
namespace m=srhd_awa::matrixgame;
namespace {
template<class T>struct OriginalDelete{void operator()(T* p)const{pas::free(p);}};
template<class T>using Owner=std::unique_ptr<T,OriginalDelete<T>>;
template<class F>void Reject(F action){bool rejected=false;try{action();}catch(...){rejected=true;}assert(rejected);}
void Empty(){
 assert(!Globals::CacheLoader && !aSaveLoad::SaveWriter && !Globals::SpaceProcess && !Globals::PrimaryFilm && !Globals::SecondaryFilm && !Globals::SpaceObjectUiLoop && !Globals::AchievementsScreen && !Globals::PlanetSpaceTemplates.length());
 GlobalsV::TGameScreenId id{};for(auto range=pas::for_to<GlobalsV::TGameScreenId>(GlobalsV::screenNone,GlobalsV::screenAchievements);range.next(id);)assert(!GlobalsV::RegisteredScreens[id]);
}
struct Caller {
 Owner<EC_BlockPar::TBlockParEC> user{pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create)};
 Owner<EC_BlockPar::TBlockParEC> new_game{pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create)};
 Owner<GR_GraphBuf::TPixelFormatGR> pixel{pas::make_object<GR_GraphBuf::TPixelFormatGR>()};
 Owner<GR_GraphBuf::TGraphBufGR> scratch{pas::construct_call<GR_GraphBuf::TGraphBufGR>(GR_GraphBuf::TGraphBufGR_Create,true)};
 Owner<GR_Main::TCCInterface> state{pas::construct_call<GR_Main::TCCInterface>(GR_Main::TCCInterface_Create)};
 Caller(){
  assert(!GR_Main::CurrentPixelFormat && !GR_Main::RenderScratchBuffer && !GR_Main::CCInterface && !GR_Main::UserSettingsConfig && !GR_Main::NewGameSettingsConfig);
  user->LoadFromTextFileWithEncodingProbe(pas::literal_pointer(u"CFG.TXT"),false);assert(user->GetParamCount()>0);
  pixel->RedMask=0xf800;pixel->GreenMask=0x07e0;pixel->BlueMask=0x001f;pixel->AlphaMask=0;pixel->BytesPerPixel=2;pixel->RebuildChannelMetrics();GR_Main::CurrentPixelFormat=pixel.get();
  GR_Main::GameScreenWidth=1024;GR_Main::GameScreenHeight=768;GR_Main::GameScreenRect={0,0,1024,768};GR_Main::ExtraScreenWidth=0;GR_Main::ExtraScreenHeight=0;
  GR_Main::UiStyleConfig=GR_Main::MainDataConfig->GetBlockByPath(u"ML"_wref.get());GR_Main::GameDataConfig=GR_Main::MainDataConfig->GetBlockByPath(u"Data"_wref.get());GR_Main::UiDepthConfig=GR_Main::MainDataConfig->GetBlockByPath(u"ZPos"_wref.get());
  const int size=GR_Main::GameDataConfig->CountParams(u"MiniMapBufSize"_wref.get())?EC_Str::ExtractDigitsToIntW(pas::view(GR_Main::GameDataConfig->GetParam(u"MiniMapBufSize"sv))):156;
  scratch->AllocateNative(size,size);GR_Main::RenderScratchBuffer=scratch.get();GR_Main::CCInterface=state.get();GR_Main::UserSettingsConfig=user.get();GR_Main::NewGameSettingsConfig=new_game.get();
 }
 ~Caller(){GR_Main::UserSettingsConfig=nullptr;GR_Main::NewGameSettingsConfig=nullptr;GR_Main::CCInterface=nullptr;GR_Main::RenderScratchBuffer=nullptr;GR_Main::CurrentPixelFormat=nullptr;GR_Main::UiStyleConfig=nullptr;GR_Main::GameDataConfig=nullptr;GR_Main::UiDepthConfig=nullptr;}
};
}
int main(int argc,char** argv){
 assert(argc==3);std::setvbuf(stdout,nullptr,_IONBF,0);srhd_awa::platform::ec_file::SetGameRoot(argv[1]);srhd_awa::platform::ec_file::SetUserRoot(argv[2]);
 Empty();Reject([]{m::RangersGlobalUiSession no_host;});Empty();
 {
  m::RangersHostOptions options;options.sound=false;options.music=false;m::RangersHostSession host(options);m::RangersScriptSession scripts;Caller caller;
  {
   Owner<fGov::TfGov> foreign{pas::construct_call<fGov::TfGov>(fGov::TfGov_Create)};Globals::GovernmentScreen=foreign.get();
   Reject([]{m::RangersGlobalUiSession duplicate;});assert(Globals::GovernmentScreen==foreign.get());Globals::GovernmentScreen=nullptr;
  }
  {
   auto* main=GR_Main::MainDataConfig;Owner<EC_BlockPar::TBlockParEC> malformed{pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create)};
   malformed->CopyFrom(main);malformed->GetBlockByPath(u"ML.Gov"_wref.get())->SetOrAddParam(u"Border"_wref.get(),u"invalid,0,1024,768"_wref.get());GR_Main::MainDataConfig=malformed.get();
   GR_Main::UiStyleConfig=malformed->GetBlockByPath(u"ML"_wref.get());GR_Main::GameDataConfig=malformed->GetBlockByPath(u"Data"_wref.get());GR_Main::UiDepthConfig=malformed->GetBlockByPath(u"ZPos"_wref.get());
   Reject([]{m::RangersGlobalUiSession partial;});Empty();assert(pas::list_count(Globals::ScriptTemplates)==0 && Globals::GlobalScriptVariables->Count==2);
   GR_Main::MainDataConfig=main;GR_Main::UiStyleConfig=main->GetBlockByPath(u"ML"_wref.get());GR_Main::GameDataConfig=main->GetBlockByPath(u"Data"_wref.get());GR_Main::UiDepthConfig=main->GetBlockByPath(u"ZPos"_wref.get());
   std::puts("GLOBAL_UI original_partial_failure_rollback=PASS unregistered_foreign_screen=REJECTED");
  }
  for(int cycle=0;cycle<2;++cycle){
   std::puts("GLOBAL_UI original_initialize_begin");
   {
    m::RangersGlobalUiSession ui;
    int screens=0;std::size_t controls=0;GlobalsV::TGameScreenId id{};
    for(auto range=pas::for_to<GlobalsV::TGameScreenId>(GlobalsV::screenNone,GlobalsV::screenAchievements);range.next(id);){
     auto* loop=pas::class_cast_if<GI_MessageLoop::TMessageLoopGI*>(GlobalsV::RegisteredScreens[id]);if(!loop)continue;++screens;assert(loop->RootUiObject);
     const auto visit=[&](auto&& self,GI_MessageLoop::TObjectGI* p)->void{for(;p;p=p->NextSibling){assert(++controls<100000 && p->MessageLoop==loop);if(p->FirstChild)self(self,p->FirstChild);}};visit(visit,loop->RootUiObject);
    }
    assert(screens>20 && controls>1000 && Globals::GovernmentScreen && Globals::HangarScreen && Globals::PrimaryFilm && Globals::SecondaryFilm && Globals::SpaceProcess);
    assert(Globals::CacheLoader && aSaveLoad::SaveWriter && Globals::SpaceObjectUiLoop && Globals::AchievementsScreen && Globals::PlanetSpaceTemplates.length()>0);
    const int scripts_count=pas::list_count(Globals::ScriptTemplates),variables=Globals::GlobalScriptVariables->Count;assert(scripts_count>0 && variables>2 && aConst::GalaxyStarCount>=73);
    Reject([]{m::RangersGlobalUiSession duplicate;});assert(!aGalaxy::Galaxy && !aPlayer::GetPlayer());
    std::printf("GLOBAL_UI cycle=%d screens=%d controls=%zu script_templates=%d global_variables=%d planet_templates=%d stars=%d original_full_initializer=PASS duplicate_owner=REJECTED campaign_generation=NOT_RUN\n",cycle,screens,controls,scripts_count,variables,Globals::PlanetSpaceTemplates.length(),aConst::GalaxyStarCount);
   }
   Empty();assert(pas::list_count(Globals::ScriptTemplates)==0 && Globals::GlobalScriptVariables->Count==2 && Globals::ReloadScriptTemplates);
   std::puts("GLOBAL_UI original_finalize_reset=PASS omitted_owners_released=PASS no_save_load=PASS");
  }
 }
 Empty();assert(!GR_Main::MainDataConfig && !GR_Main::GlobalCache && !GR_Main::CurrentPixelFormat);
}
