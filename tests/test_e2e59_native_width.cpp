#include "units/EC_Expression.hpp"
#include "units/EC_CacheFont.hpp"
#include "units/GI_MessageLoop.hpp"
#include "units/aScript.hpp"
#include "units/aScriptFun.hpp"
#include "types/ab_StopLine.hpp"
#include "types/GI_PolyLine.hpp"
#include "e2e59_action_param.hpp"
#include <cassert>
#include <array>
#include <cstdio>
#include <string>

struct Receiver {
  std::intptr_t value{};
  void Tick(GI_MessageLoop::PCallbackTimerGI, std::intptr_t data) { value = data; }
};
int main() {
  int identity{};
  const auto pointer = reinterpret_cast<std::uintptr_t>(&identity);
  assert(pointer > UINT32_MAX);
  GI_MessageLoop::TMessageLoopGI loop{};
  Receiver receiver;
  auto* timer = loop.ScheduleCallbackTimer(0, 0, pas::bind_method<&Receiver::Tick>(&receiver), pointer);
  timer->Callback(timer, timer->UserData);
  assert(std::uintptr_t(receiver.value) == pointer);
  loop.CancelCallbackTimer(timer);
  GI_PolyLine::TPolyLineSegmentGI line{};
  line.UserData = pointer;
  assert(std::uintptr_t(line.UserData) == pointer);
  ab_StopLine::TabStopLine wall{};
  wall.UserValue = pointer;
  assert(std::uintptr_t(wall.UserValue) == pointer);
  auto markup = std::u16string(u"<Object=");
  const auto digits = std::to_string(pointer);
  markup.append(digits.begin(), digits.end());
  markup += u",23,17,0>";
  EC_CacheFont::TFontObjectEC embedded{};
  assert(EC_CacheFont::TCFontEC::ParseObjectTag(markup.data(), markup.size(), embedded) == int(markup.size()));
  assert(embedded.ObjectId == pointer && embedded.Width == 23 && embedded.Height == 17);
  auto* value = pas::construct_call<EC_Expression::TVarEC>(EC_Expression::TVarEC_Create, EC_Expression::vkEmpty);
  using namespace srhd_awa::platform;
  action_param::Set(value, pointer, true);
  assert(value->Kind == EC_Expression::vkDword && std::uintptr_t(action_param::Get(value, true)) == pointer);
  action_param::Set(value, -123, false);
  assert(action_param::Get(value, false) == -123);
  Globals::ScriptActionTypeStack = pas::make_object<pas::List>();
  Globals::ScriptActionParamStack = pas::make_object<pas::List>();
  pas::list_add(Globals::ScriptActionTypeStack, reinterpret_cast<void*>(aGalaxyStruct::satOnPlayerBuyEq));
  pas::list_add(Globals::ScriptActionParamStack, &identity);
  std::array<EC_Expression::TVarEC*, 1> args{value};
  aScriptFun::SF_ScriptItemActParam({args.data(), 1}, nullptr);
  assert(value->Kind == EC_Expression::vkDword && value->GetDword() == pointer);
  pas::list_put(Globals::ScriptActionTypeStack, 0, reinterpret_cast<void*>(aGalaxyStruct::satOnWeaponShot));
  pas::list_put(Globals::ScriptActionParamStack, 0, reinterpret_cast<void*>(std::intptr_t(-123)));
  value->ResetKind(EC_Expression::vkEmpty);
  aScriptFun::SF_ScriptItemActParam({args.data(), 1}, nullptr);
  assert(value->Kind == EC_Expression::vkInt && value->GetInt() == -123);
  aScript::TScriptItem item{};
  aScript::TScriptShip binding{};
  assert(std::uintptr_t(item.RunActionCode(aGalaxyStruct::satOnPlayerBuyEq, nullptr, nullptr, nullptr, pointer)) == pointer);
  assert(std::uintptr_t(binding.RunActionCode(aGalaxyStruct::satOnPlayerBuyEq, nullptr, nullptr, nullptr, pointer)) == pointer);
  assert(item.RunActionCode(aGalaxyStruct::satOnWeaponShot, nullptr, nullptr, nullptr, -123) == -123);
  pas::free(Globals::ScriptActionTypeStack); Globals::ScriptActionTypeStack = nullptr;
  pas::free(Globals::ScriptActionParamStack); Globals::ScriptActionParamStack = nullptr;
  pas::free(value);
  std::puts("native callbacks, arcade metadata, inline markup, script purchase pointer and signed numeric events PASS");
}
