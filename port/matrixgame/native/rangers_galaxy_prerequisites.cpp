// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_galaxy_prerequisites.hpp"
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/aGalaxy.hpp"
#include "units/aScript.hpp"
#include "units/fHangar.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
namespace {
template<class T>struct OriginalDelete{void operator()(T* p)const{pas::free(p);}};
template<class T>using Owner=std::unique_ptr<T,OriginalDelete<T>>;
struct Roots {
 EC_BlockPar::TBlockParEC* previous_style=GR_Main::UiStyleConfig;
 EC_BlockPar::TBlockParEC* previous_data=GR_Main::GameDataConfig;
 EC_BlockPar::TBlockParEC* previous_depth=GR_Main::UiDepthConfig;
 EC_BlockPar::TBlockParEC* style{};EC_BlockPar::TBlockParEC* data{};EC_BlockPar::TBlockParEC* depth{};
 Roots(){
  if(!GR_Main::MainDataConfig || !GR_Main::GlobalCache || !GR_Main::CurrentPixelFormat ||
     !Globals::TurnCalculationThread || !aScript::ScriptRequestThread || !Globals::SharedScriptVariables || !aScript::ScriptFunctionScope)
   throw std::logic_error("galaxy prerequisites require real data/cache/pixel/script owners");
  if(aGalaxy::Galaxy || aGalaxy::PlayerStar || GR_Main::CCInterface || Globals::HangarScreen || GlobalsV::RegisteredScreens[GlobalsV::screenHangar])
   throw std::logic_error("galaxy prerequisites already owned or in use");
  style=GR_Main::MainDataConfig->GetBlockByPath(u"ML"_wref.get());
  data=GR_Main::MainDataConfig->GetBlockByPath(u"Data"_wref.get());
  depth=GR_Main::MainDataConfig->GetBlockByPath(u"ZPos"_wref.get());
  style->GetBlockByPath(u"Hangar"_wref.get());
  if((previous_style && previous_style!=style) || (previous_data && previous_data!=data) || (previous_depth && previous_depth!=depth))
   throw std::logic_error("galaxy UI roots owned by another context");
  GR_Main::UiStyleConfig=style;GR_Main::GameDataConfig=data;GR_Main::UiDepthConfig=depth;
 }
 ~Roots(){
  if(GR_Main::UiStyleConfig!=style || GR_Main::GameDataConfig!=data || GR_Main::UiDepthConfig!=depth)std::terminate();
  GR_Main::UiStyleConfig=previous_style;GR_Main::GameDataConfig=previous_data;GR_Main::UiDepthConfig=previous_depth;
 }
};
}
struct RangersGalaxyPrerequisites::Impl {
 Roots roots;
 Owner<fHangar::TfHangar> hangar;
 Owner<GR_Main::TCCInterface> state;
 Impl(){
  hangar.reset(pas::construct_call<fHangar::TfHangar>(fHangar::TfHangar_Create));
  hangar->InitializeFromConfig(roots.style,u"Hangar"_wref.get(),true);
  hangar->InitializeLayout();
  state.reset(pas::construct_call<GR_Main::TCCInterface>(GR_Main::TCCInterface_Create));
  if(!state->Lock || !state->Buffer || !state->SnapshotHead)throw std::runtime_error("original TCCInterface did not acquire its resources");
  Globals::HangarScreen=hangar.get();GlobalsV::RegisteredScreens[GlobalsV::screenHangar]=hangar.get();GR_Main::CCInterface=state.get();
 }
 ~Impl(){
  if(aGalaxy::Galaxy || aGalaxy::PlayerStar || Globals::HangarScreen!=hangar.get() ||
     GlobalsV::RegisteredScreens[GlobalsV::screenHangar]!=hangar.get() || GR_Main::CCInterface!=state.get())std::terminate();
  GR_Main::CCInterface=nullptr;Globals::HangarScreen=nullptr;GlobalsV::RegisteredScreens[GlobalsV::screenHangar]=nullptr;
  state.reset();hangar.reset();
 }
};
RangersGalaxyPrerequisites::RangersGalaxyPrerequisites():impl_(std::make_unique<Impl>()){}
RangersGalaxyPrerequisites::~RangersGalaxyPrerequisites()=default;
fHangar::TfHangar& RangersGalaxyPrerequisites::Hangar()const noexcept{return *impl_->hangar;}
}
