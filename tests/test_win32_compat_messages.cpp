#include "win32_compat_messages.hpp"
#include "win32_compat_window.hpp"

#include <cassert>
#include <cstdint>

using namespace srhd_awa::platform::win32_compat;

namespace {
std::int32_t g_calls = 0;
bool g_main_active = false;
std::int32_t WindowProc(std::uint32_t, std::uint32_t message,
                        std::uint32_t wparam, std::int32_t) {
  ++g_calls;
  if (message == 0x001c) g_main_active = wparam != 0;
  return static_cast<std::int32_t>(message);
}
}

int main() {
  using Peek = std::int32_t (*)(Windows::TMsg*, std::uint32_t,
                                std::uint32_t, std::uint32_t, std::uint32_t);
  using Post = std::int32_t (*)(std::uint32_t, std::uint32_t,
                                std::int32_t, std::int32_t);
  using Dispatch = std::int32_t (*)(const Windows::TMsg*);
  using Register = std::uint16_t (*)(const WindowsSdk::TWndClassW*);
  using Create = std::uint32_t (*)(std::uint32_t, char16_t*, char16_t*,
      std::uint32_t, std::int32_t, std::int32_t, std::int32_t, std::int32_t,
      std::uint32_t, std::uint32_t, std::uint32_t, void*);
  const auto peek = reinterpret_cast<Peek>(ResolveMessageImport("user32.dll", "PeekMessageW"));
  const auto post = reinterpret_cast<Post>(ResolveMessageImport("user32.dll", "PostMessageA"));
  const auto dispatch = reinterpret_cast<Dispatch>(ResolveMessageImport("user32.dll", "DispatchMessageW"));
  const auto register_class = reinterpret_cast<Register>(ResolveWindowImport("user32.dll", "RegisterClassW"));
  const auto create = reinterpret_cast<Create>(ResolveWindowImport("user32.dll", "CreateWindowExW"));
  const auto main = RegisterMainWindow(nullptr, 1280, 720);
  assert(main && MainWindow() == main);
  assert(!BindMainWindowProc(0, &WindowProc));
  assert(BindMainWindowProc(main, &WindowProc));
  assert(post(main, 0x001c, 1, 0));
  Windows::TMsg message{};
  assert(peek(&message, main, 0x001c, 0x001c, 1));
  assert(dispatch(&message) == 0x001c && g_main_active && g_calls == 1);
  WindowsSdk::TWndClassW window_class{};
  char16_t class_name[] = u"TestClass";
  window_class.lpszClassName = class_name;
  window_class.lpfnWndProc = reinterpret_cast<void*>(&WindowProc);
  assert(register_class(&window_class));
  char16_t title[] = u"Test";
  const auto child = create(0, class_name, title, 0, 10, 20, 100, 80,
                            0, 0, 0, nullptr);
  assert(child && child != main);
  assert(post(child, 0x0401, 7, 11));
  assert(peek(&message, child, 0x0401, 0x0401, 1));
  assert(message.hwnd == child && message.message == 0x0401 && message.wParam == 7);
  assert(dispatch(&message) == 0x0401 && g_calls == 2);
  srhd_awa::platform::input_platform::RawInput input{};
  input.a = true;
  InjectHostInput(input);
  bool moved = false, down = false;
  for (int i = 0; i < 4 && peek(&message, 0, 0, 0, 1); ++i) {
    moved |= message.message == 0x0200;
    down |= message.message == 0x0201;
  }
  assert(moved && down);
  input.a = false;
  InjectHostInput(input);
  assert(peek(&message, 0, 0x0202, 0x0202, 1));
  assert(message.message == 0x0202);
  input.stick_x = 32767;
  InjectHostInput(input);
  assert(peek(&message, 0, 0x0200, 0x0200, 1));
  assert((message.lParam & 0xffff) > 640);
  input.plus = true;
  InjectHostInput(input);
  assert(peek(&message, 0, 0x0012, 0x0012, 1) && message.message == 0x0012);
  UnregisterMainWindow(main);
}
