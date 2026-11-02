// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_rangers_run.hpp"
#include "init_runtime.hpp"
#include "native_form_backend.hpp"
#include "native_loop.hpp"
#include "engine_callbacks.hpp"
#include "utf16.hpp"
#include "MatrixGame.h"
#include "3g.hpp"
#include <optional>
#include <variant>
#include <stdexcept>
#include <type_traits>
namespace srhd_awa::matrixgame {
namespace {
std::optional<std::wstring> Copy(const wchar_t* text){return text?std::optional<std::wstring>(text):std::nullopt;}
const wchar_t* Pointer(const std::optional<std::wstring>& text){return text?text->c_str():nullptr;}
std::optional<std::wstring> Wide(const char16_t* text){
 if(!text)return std::nullopt;
 std::vector<std::uint8_t> bytes;
 for(;*text;++text){bytes.push_back(std::uint8_t(*text));bytes.push_back(std::uint8_t(*text>>8));}
 return ::matrixgame::native::DecodeUtf16LE(bytes);
}
}
struct NativeRangersRunBinding::State {
 std::variant<NativeGameOptions,OriginalGameOptions> options;
 std::optional<std::wstring> resource,output;
 std::optional<SRobotsSettings> settings;
 std::optional<SMGDRangersInterface> services;
 bool running{};
 explicit State(const NativeGameOptions& input):options(input),resource(Copy(input.resource_root)),output(Copy(input.output_root)){
  auto& owned=std::get<NativeGameOptions>(options);owned.resource_root=Pointer(resource);owned.output_root=Pointer(output);
  if(input.settings){settings=*input.settings;owned.settings=&*settings;}
  if(input.services){services=*input.services;owned.services=&*services;}
 }
 explicit State(const OriginalGameOptions& input):options(input),resource(Copy(input.resource_root)){
  if(!input.window_provider || !input.device_provider)throw std::invalid_argument("Rangers reference Run requires paired real providers");
  auto& owned=std::get<OriginalGameOptions>(options);owned.resource_root=Pointer(resource);
  if(input.settings){settings=*input.settings;owned.settings=&*settings;}
 }
};
thread_local NativeRangersRunBinding::State* NativeRangersRunBinding::active_{};
NativeRangersRunBinding::NativeRangersRunBinding(const NativeGameOptions& input){
 if(active_)throw std::logic_error("Rangers native Run already bound on this thread");
 state_=std::make_unique<State>(input);active_=state_.get();
}
NativeRangersRunBinding::NativeRangersRunBinding(const OriginalGameOptions& input,RangersLegacyReferenceTag){
 if(active_)throw std::logic_error("Rangers native Run already bound on this thread");
 state_=std::make_unique<State>(input);active_=state_.get();
}
NativeRangersRunBinding::~NativeRangersRunBinding(){if(active_!=state_.get() || state_->running)std::terminate();active_=nullptr;}
Robot::TRobotRun NativeRangersRunBinding::Entry()const noexcept{return Robot::TRobotRun(&Run);}
std::int32_t PAS_STDCALL NativeRangersRunBinding::Run(std::uint32_t module,std::uint32_t window,
 char16_t* map,Robot::PRobotDisplaySettings input,char16_t* language,char16_t* start,char16_t* win,
 char16_t* loss,char16_t* planet,aGalaxyStruct::PPlanetBattleStatistics output){
 // Legacy caller tokens are not pointers. Full provider identities come only
 // from the explicitly registered native/reference options, without truncation.
 (void)module;(void)window;
 if(!active_)throw std::logic_error("Rangers native Run has no scoped provider binding");
 if(active_->running)throw std::logic_error("recursive Rangers native Run");
 if(!output)throw std::invalid_argument("Rangers native Run requires result storage");
 auto& state=*active_;state.running=true;
 struct Running {bool& value;~Running(){value=false;}} running{state.running};
 const auto map_w=Wide(map),language_w=Wide(language),start_w=Wide(start),win_w=Wide(win),loss_w=Wide(loss),planet_w=Wide(planet);
 std::optional<SRobotsSettings> settings;
 if(input)settings=ToEngineSettings(ReadSettings(*input));
 NativeInitRuntime runtime;
 std::visit([&](const auto& configured){
  auto options=configured;options.map=Pointer(map_w);options.language=Pointer(language_w);
  options.start_text=Pointer(start_w);options.win_text=Pointer(win_w);options.loss_text=Pointer(loss_w);options.planet=Pointer(planet_w);
  if(settings)options.settings=&*settings;
  if constexpr(std::is_same_v<std::decay_t<decltype(options)>,NativeGameOptions>)runtime.InitializeNativeGame(options);
  else runtime.InitializeOriginalGame(options);
 },state.options);
 {
  native_form::Session form;g_ExitState=0;g_Flags&=~GFLAG_EXITLOOP;
  RunOriginalNativeLoop();
 }
 const int code=(g_Flags&GFLAG_EXITLOOP)?g_ExitState:0;
 SRobotGameState snapshot{};runtime.SaveOriginalGameResult(snapshot);
 const auto statistics=ReadEngineStatistics(snapshot);
 runtime.Deinit();const auto restored=runtime.LastStateRestoreResult();
 if(!restored)throw std::runtime_error("Rangers native Run has no original device restore result");
 platform3g::CheckDeviceResult(*restored,"Rangers native Run device restore");
 WriteStatistics(statistics,*output);return code;
}
}
