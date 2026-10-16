#pragma once
#include "units/EC_Expression.hpp"
#include "types/aGalaxyStruct.hpp"
#include "units/Globals.hpp"

namespace srhd_awa::platform::action_param {
inline bool IsPointer(aGalaxyStruct::TScriptActionType type) {
  return type == aGalaxyStruct::satOnPlayerBuyEq;
}
inline bool CurrentIsPointer() {
  const auto count = pas::list_count(Globals::ScriptActionTypeStack);
  return count && IsPointer(static_cast<aGalaxyStruct::TScriptActionType>(
      reinterpret_cast<std::uintptr_t>(pas::list_get(Globals::ScriptActionTypeStack, count - 1))));
}
inline std::intptr_t Get(EC_Expression::TVarEC* value, bool pointer) {
  return pointer ? static_cast<std::intptr_t>(value->GetDword()) : value->GetInt();
}
inline void Set(EC_Expression::TVarEC* value, std::intptr_t data, bool pointer) {
  if (pointer) value->SetDword(static_cast<std::uintptr_t>(data));
  else value->SetInt(static_cast<std::int32_t>(data));
}
}
