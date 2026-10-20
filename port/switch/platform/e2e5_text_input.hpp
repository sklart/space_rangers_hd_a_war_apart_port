#pragma once
#include "runtime_support.hpp"
#include "controller_text_input.hpp"
#include "win32_compat_messages.hpp"
#include <cstring>
#include <vector>
#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace srhd_awa::platform::e2e5_text_input {
inline thread_local controller_text_input::Backend injected_backend{};
inline thread_local void* backend_user{};
inline void SetHostBackend(controller_text_input::Backend backend, void* user = nullptr) {
  injected_backend = backend;
  backend_user = user;
}
inline bool NativeBackend(const controller_text_input::Request& request,
                          std::u16string& value, void*) {
#if defined(__SWITCH__)
  std::vector<char> initial(request.initial.size() * 3 + 1);
  const auto bytes = utf16_to_utf8(reinterpret_cast<std::uint8_t*>(initial.data()),
      reinterpret_cast<const std::uint16_t*>(request.initial.c_str()), initial.size() - 1);
  if (bytes < 0 || static_cast<std::size_t>(bytes) >= initial.size()) return false;
  initial[bytes] = 0;
  std::vector<char> output(request.max_length * 3 + 1);
  std::vector<std::uint16_t> wide(request.max_length + 1);
  SwkbdConfig keyboard{};
  if (R_FAILED(swkbdCreate(&keyboard, 0))) return false;
  struct Close { SwkbdConfig* config; ~Close() { swkbdClose(config); } } close{&keyboard};
  swkbdConfigMakePresetDefault(&keyboard);
  swkbdConfigSetStringLenMin(&keyboard, request.min_length);
  swkbdConfigSetStringLenMax(&keyboard, request.max_length);
  swkbdConfigSetHeaderText(&keyboard, request.header);
  swkbdConfigSetInitialText(&keyboard, initial.data());
  if (R_FAILED(swkbdShow(&keyboard, output.data(), output.size()))) return false;
  const auto count = utf8_to_utf16(wide.data(),
      reinterpret_cast<const std::uint8_t*>(output.data()), request.max_length);
  if (count < 0 || static_cast<std::size_t>(count) > request.max_length) return false;
  value.assign(reinterpret_cast<const char16_t*>(wide.data()), count);
  return true;
#else
  (void)request; (void)value;
  return false;
#endif
}
inline bool EditText(pas::WideString& value, std::size_t minimum, std::size_t maximum,
                     const char* header = "Введите текст") {
#if !defined(__SWITCH__)
  if (!injected_backend) return false; // Preserve the ordinary host keyboard driver.
#endif
  struct Suppress { ~Suppress() { input_platform::SuppressUntilReleased(); } } suppress;
  win32_compat::InputContextScope text(controller_actions::Context::TextEntry, nullptr, nullptr);
  win32_compat::InputContextScope applet(controller_actions::Context::SwitchAppletActive, nullptr, nullptr);
  std::u16string candidate(value.c_str(), value.length());
  if (!controller_text_input::Prompt(candidate, minimum, maximum, header,
          injected_backend ? injected_backend : &NativeBackend, backend_user)) return false;
  pas::WideString replacement(candidate.c_str());
  value = std::move(replacement);
  return true;
}
inline bool EditSaveName(pas::WideString& value) { return EditText(value, 1, 55, "Имя сохранения"); }
inline bool EditPlayerName(pas::WideString& value) { return EditText(value, 1, 13, "Имя персонажа"); }
} // namespace srhd_awa::platform::e2e5_text_input
