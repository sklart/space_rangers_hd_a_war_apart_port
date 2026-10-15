#pragma once
#include "runtime_support.hpp"
#include "e2e_stage.hpp"
#include "input_platform.hpp"
#include <cstring>
#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace srhd_awa::platform::e2e5_text_input {
inline bool EditPlayerName(pas::WideString& name) {
#if defined(__SWITCH__)
  SwkbdConfig keyboard{};
  const Result created = swkbdCreate(&keyboard, 0);
  if (R_FAILED(created)) {
    e2e_stage::Log("keyboard create failed");
    return false;
  }
  swkbdConfigMakePresetDefault(&keyboard);
  swkbdConfigSetStringLenMin(&keyboard, 1);
  swkbdConfigSetStringLenMax(&keyboard, 13);
  swkbdConfigSetHeaderText(&keyboard, "Имя персонажа");
  char initial[128]{};
  const auto bytes = utf16_to_utf8(reinterpret_cast<std::uint8_t*>(initial),
      reinterpret_cast<const std::uint16_t*>(name.c_str()), sizeof(initial)-1);
  if (bytes >= 0 && bytes < static_cast<ssize_t>(sizeof(initial)))
    swkbdConfigSetInitialText(&keyboard, initial);
  char output[128]{};
  const Result result = swkbdShow(&keyboard, output, sizeof(output));
  swkbdClose(&keyboard);
  input_platform::SuppressUntilReleased();
  if (R_FAILED(result)) {
    e2e_stage::Log("keyboard canceled or unavailable");
    return false;
  }
  std::uint16_t wide[14]{};
  const auto count = utf8_to_utf16(wide,
      reinterpret_cast<const std::uint8_t*>(output), 13);
  if (count < 1 || count > 13) return false;
  name.set_length(static_cast<std::int32_t>(count));
  std::memcpy(name.pchar(), wide, static_cast<std::size_t>(count)*2);
  e2e_stage::Log("keyboard player name accepted");
  return true;
#else
  return false;
#endif
}
} // namespace srhd_awa::platform::e2e5_text_input
