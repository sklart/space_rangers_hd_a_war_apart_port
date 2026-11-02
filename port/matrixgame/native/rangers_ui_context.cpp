// SPDX-License-Identifier: GPL-2.0-or-later
#include "rangers_ui_context.hpp"
#include "runtime_support.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Str.hpp"
#include "units/EC_Mem.hpp"
#include "units/EC_Cache.hpp"
#include "units/GI_Main.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/GR_DX.hpp"
#include "units/Globals.hpp"
#include "units/GlobalsV.hpp"
#include "units/aGalaxy.hpp"
#include "units/aMyFunction.hpp"
#include "units/aScript.hpp"
#include "renderer_platform.hpp"
#include "user_root.hpp"
#include <codecvt>
#include <locale>
#include <stdexcept>
#include <new>
namespace srhd_awa::matrixgame {
namespace renderer=srhd_awa::platform::renderer_platform;
namespace {
bool owned{};
template<class T>struct OriginalDelete {void operator()(T* p)const{pas::free(p);}};
template<class T>using Owner=std::unique_ptr<T,OriginalDelete<T>>;
template<class T>struct Saved {T& target;T old;explicit Saved(T& value):target(value),old(value){}~Saved(){target=old;}};
struct SavedGlobals {
#define KEEP(ns,name) Saved<decltype(ns::name)> name{ns::name};
 KEEP(GR_Main,UiStyleConfig) KEEP(GR_Main,GameDataConfig) KEEP(GR_Main,UiDepthConfig)
 KEEP(GR_Main,UserSettingsConfig) KEEP(GR_Main,NewGameSettingsConfig) KEEP(GR_Main,CCInterface)
 KEEP(GR_Main,CachedGameUserDirectory)
 KEEP(GR_Main,GameScreenWidth) KEEP(GR_Main,GameScreenHeight) KEEP(GR_Main,GameScreenRect)
 KEEP(GR_Main,ExtraScreenWidth) KEEP(GR_Main,ExtraScreenHeight)
 KEEP(GR_Main,PresentationWidth) KEEP(GR_Main,PresentationHeight) KEEP(GR_Main,PresentationRect)
 KEEP(GR_Main,ScreenCenterX) KEEP(GR_Main,ScreenCenterY)
 KEEP(GR_Main,DesktopDisplayMode) KEEP(GR_Main,GameDisplayModes) KEEP(GR_Main,RobotDisplayModes)
 KEEP(GR_Main,GameDisplayModeCount) KEEP(GR_Main,RobotDisplayModeCount)
 KEEP(GR_Main,SelectedGameDisplayMode) KEEP(GR_Main,SelectedRobotDisplayMode) KEEP(GR_Main,SmallestGameDisplayMode)
 KEEP(GR_Main,Direct3DPresentParameters) KEEP(GR_Main,VSyncEnabled) KEEP(GR_Main,PresentWithoutLimit) KEEP(GR_Main,PresentationFrameRate)
 KEEP(GR_Main,BlendPixel16) KEEP(GR_Main,TriangleRasterizer16) KEEP(GR_Main,LineRasterizer16)
 KEEP(GlobalsV,ScaleViewportToWindow) KEEP(GlobalsV,HardwareRenderingEnabled)
 KEEP(GlobalsV,PlanetDepth) KEEP(GlobalsV,ShipPathDepth) KEEP(GlobalsV,ShipPathEndDepth)
 KEEP(GlobalsV,UnitPathDepth) KEEP(GlobalsV,UnitPathEndDepth) KEEP(GlobalsV,ActionButtonDepth)
 KEEP(GlobalsV,GalaxyStarDepth) KEEP(GlobalsV,GalaxyStarNameDepth) KEEP(GlobalsV,GalaxyWarDepth)
 KEEP(GlobalsV,ConstellationLineDepth) KEEP(GlobalsV,ConstellationColorDepth)
 KEEP(aMyFunction,InfoNameColorTag) KEEP(aMyFunction,InfoHullSeriesColorTag)
 KEEP(GR_DX,CircleCos) KEEP(GR_DX,CircleSin) KEEP(GR_DX,LineAlphaTable)
 KEEP(GR_DX,PendingPoints) KEEP(GR_DX,PendingPointCount) KEEP(GR_DX,PendingPointCapacity)
#undef KEEP
};
bool ScreensEmpty(){
 GlobalsV::TGameScreenId id{};
 for(auto range=pas::for_to<GlobalsV::TGameScreenId>(GlobalsV::screenNone,GlobalsV::screenAchievements);range.next(id);)if(GlobalsV::RegisteredScreens[id])return false;
 return true;
}
void RequireIdle(){
 if(owned || renderer::IsInitialized() || GR_Main::CurrentPixelFormat || GR_Main::ScreenRenderBuffer || GR_Main::RenderScratchBuffer || GR_Main::AuxRenderBuffer ||
    GR_Main::UserSettingsConfig || GR_Main::NewGameSettingsConfig || GR_Main::CCInterface || GR_Main::UiStyleConfig || GR_Main::GameDataConfig || GR_Main::UiDepthConfig ||
    GR_Main::InterfaceBlendPalette || GR_Main::FirstRegisteredCursor || GR_Main::LastRegisteredCursor ||
    Globals::TurnCalculationThread || aScript::ScriptRequestThread || aGalaxy::Galaxy || !ScreensEmpty() || GR_DX::PendingPoints.length() || GR_DX::PendingPointCount)
  throw std::logic_error("UI context requires cold unowned globals before script workers");
 if(!GR_Main::MainDataConfig || !GR_Main::GlobalCache)throw std::logic_error("UI context requires the real host data/cache owner");
}
void CheckOptions(const RangersUiContextOptions& o){
 if(o.settings_path.empty() || o.settings_path.find(u'\0')!=o.settings_path.npos || o.new_game_settings_path.find(u'\0')!=o.new_game_settings_path.npos ||
    o.width<1024 || o.height<768 || o.width>16384 || o.height>16384 || o.presentation_width<=0 || o.presentation_height<=0 || o.presentation_width>16384 || o.presentation_height>16384)
  throw std::invalid_argument("invalid real UI config path or canvas geometry");
}
}
struct RangersUiContext::Impl {
 SavedGlobals saved;
 Owner<EC_BlockPar::TBlockParEC> user,new_game;
 Owner<GR_Main::TCCInterface> state;
 bool renderer_owned{},palette_owned{},ready{};
 GR_GraphBuf::TPixelFormatGR* pixel{};
 explicit Impl(const RangersUiContextOptions& o){
  owned=true;
  try{
   const auto directory=srhd_awa::platform::user_root::ConfigDirectory();
   if(directory.empty() || !srhd_awa::platform::user_root::EnsureLayout())throw std::runtime_error("UI context requires a real separate user-root layout");
   const auto wide=std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t>{}.from_bytes(directory);
   // Original GetGameUserDirectory reads this cache first. Keep its real path
   // semantics without falling back to desktop Documents or writing settings.
   GR_Main::CachedGameUserDirectory=pas::WideString(std::u16string_view(wide));
   user.reset(pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create));
   new_game.reset(pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create));
   const pas::WideString path(std::u16string_view(o.settings_path));user->LoadFromTextFileWithEncodingProbe(path.pchar(),false);
   if(user->GetParamCount()<=0)throw std::runtime_error("original UI settings config is empty");
   if(!o.new_game_settings_path.empty()){const pas::WideString path(std::u16string_view(o.new_game_settings_path));new_game->LoadFromTextFileWithEncodingProbe(path.pchar(),false);}
   state.reset(pas::construct_call<GR_Main::TCCInterface>(GR_Main::TCCInterface_Create));
   GR_Main::UserSettingsConfig=user.get();GR_Main::NewGameSettingsConfig=new_game.get();GR_Main::CCInterface=state.get();
   GR_Main::UiStyleConfig=GR_Main::MainDataConfig->GetBlockByPath(u"ML"_wref.get());
   GR_Main::GameDataConfig=GR_Main::MainDataConfig->GetBlockByPath(u"Data"_wref.get());
   GR_Main::UiDepthConfig=GR_Main::MainDataConfig->GetBlockByPath(u"ZPos"_wref.get());
   GR_Main::LoadInformationColorTags();
#define DEPTH(field,key) GlobalsV::field=EC_Str::ExtractDecimalToSingleW(GR_Main::UiDepthConfig->GetParam(u##key##sv));
   DEPTH(PlanetDepth,"Planet") DEPTH(ShipPathDepth,"UnitPathShip") DEPTH(ShipPathEndDepth,"UnitPathEndShip")
   DEPTH(UnitPathDepth,"UnitPath") DEPTH(UnitPathEndDepth,"UnitPathEnd") DEPTH(ActionButtonDepth,"ButtonAction")
   DEPTH(GalaxyStarDepth,"GalaxyStar") DEPTH(GalaxyStarNameDepth,"GalaxyStarName") DEPTH(GalaxyWarDepth,"GalaxyWar")
   DEPTH(ConstellationLineDepth,"ConstellationLine") DEPTH(ConstellationColorDepth,"ConstellationColor")
#undef DEPTH
   renderer::RendererConfig config;config.game_width=o.width;config.game_height=o.height;config.presentation_width=o.presentation_width;config.presentation_height=o.presentation_height;config.frame_limit=false;
   if(GR_Main::GameDataConfig->CountParams(u"MiniMapBufSize"_wref.get()))config.minimap_buffer_size=EC_Str::ExtractDigitsToIntW(pas::view(GR_Main::GameDataConfig->GetParam(u"MiniMapBufSize"sv)));
   std::string error;if(!renderer::InitializeSoftwareRenderer(config,&error))throw std::runtime_error(error);renderer_owned=true;pixel=GR_Main::CurrentPixelFormat;
   GR_Main::ScreenCenterX=o.width/2;GR_Main::ScreenCenterY=o.height/2;GR_Main::Direct3DPresentParameters.Windowed=true;
   GR_Main::InterfaceBlendPalette=EC_Mem::AllocEC(512);if(!GR_Main::InterfaceBlendPalette)throw std::bad_alloc();palette_owned=true;
   for(int i=0;i<256;++i){auto* item=EC_Mem::AddPointerOffset(GR_Main::InterfaceBlendPalette,2*i);EC_Mem::WriteWordEC(item,pixel->PackRgbBytes(8,32,255));GR_Main::BlendPixel16(item,pixel->PackRgbBytes(200,128,128),i);}
   auto* cursors=GR_Main::MainDataConfig->GetBlockByPath(u"Graph.Cursor"_wref.get());
   for(int i=0;i<cursors->GetBlockCount();++i){auto* cursor=GR_Main::AddCursorUnit();cursor->Name=cursors->GetBlockNameByIndex(i);cursor->ImagePath=cursors->GetBlockByIndex(i)->GetParam(u"Image"sv);cursor->HotSpot=GI_Main::GetPointGI(pas::view(cursors->GetBlockByIndex(i)->GetParam(u"Sme"sv)));}
   ready=true;
  }catch(...){Clear();throw;}
 }
 void Clear()noexcept{
  if(Globals::TurnCalculationThread || aScript::ScriptRequestThread || aGalaxy::Galaxy || !ScreensEmpty())std::terminate();
  if(ready && (!owned || GR_Main::UserSettingsConfig!=user.get() || GR_Main::CCInterface!=state.get() || GR_Main::NewGameSettingsConfig!=new_game.get() || GR_Main::CurrentPixelFormat!=pixel))std::terminate();
  try{
   while(GR_Main::FirstRegisteredCursor)GR_Main::RemoveCursorUnit(GR_Main::FirstRegisteredCursor);
   if(palette_owned){EC_Mem::FreeEC(GR_Main::InterfaceBlendPalette);GR_Main::InterfaceBlendPalette=nullptr;palette_owned=false;}
   if(renderer_owned){GR_Main::GlobalCache->TrimToBudget(0);renderer::ShutdownSoftwareRenderer();renderer_owned=false;}
   GR_Main::UserSettingsConfig=nullptr;GR_Main::NewGameSettingsConfig=nullptr;GR_Main::CCInterface=nullptr;
   GR_Main::UiStyleConfig=nullptr;GR_Main::GameDataConfig=nullptr;GR_Main::UiDepthConfig=nullptr;
   state.reset();new_game.reset();user.reset();ready=false;owned=false;
  }catch(...){std::terminate();}
 }
 ~Impl(){Clear();}
};
RangersUiContext::RangersUiContext(const RangersUiContextOptions& options){CheckOptions(options);RequireIdle();impl_=std::make_unique<Impl>(options);}
RangersUiContext::~RangersUiContext()=default;
EC_BlockPar::TBlockParEC& RangersUiContext::Settings()const noexcept{return *impl_->user;}
}
