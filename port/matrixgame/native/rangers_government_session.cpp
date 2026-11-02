// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_government_session.hpp"
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/GR_Main.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/fGov.hpp"
#include <stdexcept>
namespace srhd_awa::matrixgame {
namespace {
struct UiRoots {
 EC_BlockPar::TBlockParEC* previous_style=GR_Main::UiStyleConfig;
 EC_BlockPar::TBlockParEC* previous_data=GR_Main::GameDataConfig;
 EC_BlockPar::TBlockParEC* previous_depth=GR_Main::UiDepthConfig;
 EC_BlockPar::TBlockParEC* style{};EC_BlockPar::TBlockParEC* data{};EC_BlockPar::TBlockParEC* depth{};
 UiRoots(){
  if(!GR_Main::MainDataConfig || !GR_Main::GlobalCache || !GR_Main::CurrentPixelFormat)
   throw std::logic_error("government UI requires real Rangers data/cache/pixel-format owners");
  style=GR_Main::MainDataConfig->GetBlockByPath(u"ML"_wref.get());
  data=GR_Main::MainDataConfig->GetBlockByPath(u"Data"_wref.get());
  depth=GR_Main::MainDataConfig->GetBlockByPath(u"ZPos"_wref.get());
  // Resolve required real data before publishing any borrowed pointer.
  style->GetBlockByPath(u"Gov"_wref.get());
  if((previous_style && previous_style!=style) || (previous_data && previous_data!=data) || (previous_depth && previous_depth!=depth))
   throw std::logic_error("government UI roots owned by another context");
  GR_Main::UiStyleConfig=style;GR_Main::GameDataConfig=data;GR_Main::UiDepthConfig=depth;
 }
 ~UiRoots(){
  if(GR_Main::UiStyleConfig!=style || GR_Main::GameDataConfig!=data || GR_Main::UiDepthConfig!=depth)std::terminate();
  GR_Main::UiStyleConfig=previous_style;GR_Main::GameDataConfig=previous_data;GR_Main::UiDepthConfig=previous_depth;
 }
};
struct OriginalDelete {void operator()(fGov::TfGov* p)const{pas::free(p);}};
}
struct RangersGovernmentSession::Impl {
 UiRoots roots;
 std::unique_ptr<fGov::TfGov,OriginalDelete> screen;
 bool installed{};
 Impl(){
  if(Globals::GovernmentScreen || GlobalsV::RegisteredScreens[GlobalsV::screenGovernment])
   throw std::logic_error("government screen already owned");
  screen.reset(pas::construct_call<fGov::TfGov>(fGov::TfGov_Create));
  screen->InitializeFromConfig(roots.style,u"Gov"_wref.get(),true);
  screen->InitializeLayout();
  Globals::GovernmentScreen=screen.get();
  GlobalsV::RegisteredScreens[GlobalsV::screenGovernment]=screen.get();installed=true;
 }
 ~Impl(){
  if(installed){
   if(Globals::GovernmentScreen!=screen.get() || GlobalsV::RegisteredScreens[GlobalsV::screenGovernment]!=screen.get())std::terminate();
   Globals::GovernmentScreen=nullptr;GlobalsV::RegisteredScreens[GlobalsV::screenGovernment]=nullptr;
  }
  screen.reset(); // Original destructors still have borrowed style/data roots.
 }
};
RangersGovernmentSession::RangersGovernmentSession():impl_(std::make_unique<Impl>()){}
RangersGovernmentSession::~RangersGovernmentSession()=default;
fGov::TfGov& RangersGovernmentSession::Screen()const noexcept{return *impl_->screen;}
}
