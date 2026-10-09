#include "win32_compat_messages.hpp"

#include "e2e_clock.hpp"
#include "e2e_events.hpp"
#include "e2e_stage.hpp"
#include "win32_compat_window.hpp"
#include "win32_handles.hpp"
#include "types/Windows_group.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(__SWITCH__)
#include <SDL2/SDL.h>
#endif

namespace srhd_awa::platform::win32_compat {
namespace {
constexpr std::uint32_t kWmQuit = 0x0012u;
constexpr std::uint32_t kWmPaint = 0x000fu;
constexpr std::uint32_t kWmTimer = 0x0113u;
constexpr std::uint32_t kWmMouseMove = 0x0200u;
constexpr std::uint32_t kWmLeftDown = 0x0201u;
constexpr std::uint32_t kWmLeftUp = 0x0202u;
constexpr std::uint32_t kWmRightDown = 0x0204u;
constexpr std::uint32_t kWmRightUp = 0x0205u;
constexpr std::uint32_t kWmKeyDown = 0x0100u;
constexpr std::uint32_t kWmKeyUp = 0x0101u;
constexpr std::uint32_t kWmChar = 0x0102u;
constexpr std::uint32_t kVkReturn = 0x0du;
constexpr std::uint32_t kVkEscape = 0x1bu;
constexpr std::uint32_t kVkLeftMouse = 1u;
constexpr std::uint32_t kVkRightMouse = 2u;
constexpr std::uint32_t kWaitFailed = 0xffffffffu;
constexpr std::uint32_t kWaitTimeout = 258u;
struct Timer {
  std::uint32_t window = 0, id = 0, interval = 0;
  std::uint64_t due = 0;
};
std::mutex g_mutex;
std::deque<Windows::TMsg> g_queue;
std::unordered_map<std::string, std::uint32_t> g_registered;
std::uint32_t g_next_registered = 0xc000u;
std::vector<std::shared_ptr<Timer>> g_timers;
input_platform::State g_input;
std::array<bool, 256> g_keys{};
std::int32_t g_last_x = -1, g_last_y = -1;
#if defined(__SWITCH__)
std::uint32_t g_last_input_diagnostic_tick{};
bool g_input_diagnostic_started{};
#endif

std::int32_t MouseLParam(std::int32_t x, std::int32_t y) {
  return static_cast<std::int32_t>((static_cast<std::uint32_t>(y) & 0xffffu) << 16 |
                                    (static_cast<std::uint32_t>(x) & 0xffffu));
}

void Enqueue(std::uint32_t window, std::uint32_t message,
             std::uint32_t wparam = 0, std::int32_t lparam = 0,
             std::int32_t x = 0, std::int32_t y = 0) {
  Windows::TMsg item{};
  item.hwnd = window;
  item.message = message;
  item.wParam = wparam;
  item.lParam = lparam;
  item.time = e2e_clock::Milliseconds();
  item.pt = {x, y};
  g_queue.push_back(item);
}

void Pump() {
  const auto window = MainWindow();
  if (!window) return;
#if defined(__SWITCH__)
  SDL_Event event;
  while (SDL_PollEvent(&event))
    if (event.type == SDL_QUIT) {
      std::lock_guard lock(g_mutex);
      Enqueue(0, kWmQuit);
    }
#endif
  std::int32_t width = 1280, height = 720;
  MainWindowSize(&width, &height);
  const auto input = input_platform::Poll(&g_input, width, height);
  SetInputCursor(input.x, input.y);
#if defined(__SWITCH__)
  const auto diagnostic_tick = e2e_clock::Milliseconds();
  const bool button_edge = input.left_down || input.left_up || input.right_down ||
      input.right_up || input.enter_down || input.enter_up || input.escape_down ||
      input.escape_up || input.plus_down;
  if (!g_input_diagnostic_started || button_edge ||
      diagnostic_tick - g_last_input_diagnostic_tick >= 5000u) {
    char detail[160]{};
    std::snprintf(detail, sizeof(detail),
        "input poll window=%u size=%dx%d cursor=%d,%d buttons=A%d B%d X%d Y%d PLUS%d",
        window, width, height, input.x, input.y,
        input.left_held, input.right_held, input.enter_held,
        input.escape_held, input.plus_down);
    e2e_stage::Log(detail);
    g_last_input_diagnostic_tick = diagnostic_tick;
    g_input_diagnostic_started = true;
  }
#endif
  std::lock_guard lock(g_mutex);
  const auto lparam = MouseLParam(input.x, input.y);
  if (input.x != g_last_x || input.y != g_last_y) {
    Enqueue(window, kWmMouseMove, 0, lparam, input.x, input.y);
    g_last_x = input.x; g_last_y = input.y;
  }
  if (input.left_down) Enqueue(window, kWmLeftDown, 1, lparam, input.x, input.y);
  if (input.left_up) Enqueue(window, kWmLeftUp, 0, lparam, input.x, input.y);
  if (input.right_down) Enqueue(window, kWmRightDown, 2, lparam, input.x, input.y);
  if (input.right_up) Enqueue(window, kWmRightUp, 0, lparam, input.x, input.y);
  auto key = [&](std::uint32_t vk, bool down, bool up, bool held) {
    g_keys[vk] = held;
    if (down) Enqueue(window, kWmKeyDown, vk);
    if (up) Enqueue(window, kWmKeyUp, vk);
  };
  key(kVkReturn, input.enter_down, input.enter_up, input.enter_held);
  key(kVkEscape, input.escape_down, input.escape_up, input.escape_held);
  g_keys[kVkLeftMouse] = input.left_held;
  g_keys[kVkRightMouse] = input.right_held;
  if (input.plus_down) Enqueue(0, kWmQuit); // PLUS is process exit, never a VK.
  const auto now = static_cast<std::uint64_t>(e2e_clock::Milliseconds());
  for (const auto& timer : g_timers) {
    if (now >= timer->due) {
      Enqueue(timer->window, kWmTimer, timer->id);
      timer->due = now + timer->interval;
    }
  }
}

std::int32_t PostMessage(std::uint32_t window, std::uint32_t message,
                          std::int32_t wparam, std::int32_t lparam) {
  if (window && !Handles().IsValid(window, HandleType::Window)) return 0;
  std::lock_guard lock(g_mutex);
  Enqueue(window, message, static_cast<std::uint32_t>(wparam), lparam);
  SetLastError(kErrorSuccess);
  return 1;
}

std::int32_t PeekMessage(Windows::TMsg* output, std::uint32_t window,
                          std::uint32_t first, std::uint32_t last,
                          std::uint32_t remove) {
  if (!output) { SetLastError(kErrorInvalidParameter); return 0; }
  Pump();
  std::lock_guard lock(g_mutex);
  auto found = std::find_if(g_queue.begin(), g_queue.end(), [&](const auto& item) {
    const bool hwnd_match = !window || item.hwnd == window || item.message == kWmQuit;
    const bool filter_match = (!first && !last) ||
        (item.message >= first && item.message <= last) || item.message == kWmQuit;
    return hwnd_match && filter_match;
  });
  if (found == g_queue.end()) return 0;
  *output = *found;
  if (remove & 1u) g_queue.erase(found);
  SetLastError(kErrorSuccess);
  return 1;
}

std::int32_t TranslateMessage(const Windows::TMsg* message) {
  if (!message) { SetLastError(kErrorInvalidParameter); return 0; }
  if (message->message != kWmKeyDown) return 0;
  const auto vk = message->wParam;
  const auto character = vk == kVkReturn ? 13u : vk >= 32 && vk <= 126 ? vk : 0u;
  if (!character) return 0;
  std::lock_guard lock(g_mutex);
  Enqueue(message->hwnd, kWmChar, character, message->lParam);
  return 1;
}

std::int32_t DispatchMessage(const Windows::TMsg* message) {
  if (!message) { SetLastError(kErrorInvalidParameter); return 0; }
  return message->message == kWmQuit ? 0 : DispatchWindowMessage(*message);
}

std::int32_t DefWindowProc(std::uint32_t, std::uint32_t message,
                            std::int32_t, std::int32_t) {
  return message == 0x0010u ? 0 : 0; // WM_CLOSE is consumed by caller's loop.
}

void PostQuit(std::int32_t code) {
  std::lock_guard lock(g_mutex);
  Enqueue(0, kWmQuit, static_cast<std::uint32_t>(code));
}

std::uint32_t RegisterWindowMessage(std::uint8_t* name) {
  if (!name || !*name) { SetLastError(kErrorInvalidParameter); return 0; }
  std::lock_guard lock(g_mutex);
  const std::string key(reinterpret_cast<char*>(name));
  if (const auto found = g_registered.find(key); found != g_registered.end())
    return found->second;
  if (g_next_registered > 0xffffu) { SetLastError(kErrorInvalidParameter); return 0; }
  const auto value = g_next_registered++;
  g_registered.emplace(key, value);
  return value;
}

std::uint32_t SetTimer(std::uint32_t window, std::uint32_t id,
                        std::uint32_t interval, void* callback) {
  if (callback || !window || !Handles().IsValid(window, HandleType::Window)) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  auto timer = std::make_shared<Timer>();
  timer->window = window;
  timer->interval = std::max(interval, 1u);
  timer->due = static_cast<std::uint64_t>(e2e_clock::Milliseconds()) + timer->interval;
  timer->id = id ? id : Handles().Allocate(HandleType::Timer, timer);
  std::lock_guard lock(g_mutex);
  g_timers.push_back(timer);
  SetLastError(kErrorSuccess);
  return timer->id;
}

std::uint32_t MsgWait(std::uint32_t count, const std::uint32_t* tokens,
                       std::int32_t wait_all, std::uint32_t timeout,
                       std::uint32_t) {
  if ((count && !tokens) || count > 64) {
    SetLastError(kErrorInvalidParameter);
    return kWaitFailed;
  }
  if (wait_all && count) {
    SetLastError(kErrorInvalidParameter);
    e2e_stage::LogWinApi("UNSUPPORTED operation=MsgWaitForMultipleObjects wait-all");
    return kWaitFailed;
  }
  const auto start = e2e_clock::Milliseconds();
  while (true) {
    Pump();
    {
      std::lock_guard lock(g_mutex);
      if (!g_queue.empty()) return count;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
      const auto object = Handles().Lookup(tokens[index], HandleType::Event);
      if (!object) return kWaitFailed;
      const auto backend = *static_cast<std::uint32_t*>(object.get());
      if (e2e_events::WaitOne(backend, 0) == 0) return index;
    }
    if (timeout != 0xffffffffu && e2e_clock::Milliseconds() - start >= timeout)
      return kWaitTimeout;
    e2e_clock::SleepMilliseconds(1);
  }
}

std::int16_t GetAsyncKeyState(std::int32_t vk) {
  if (vk < 0 || vk >= static_cast<std::int32_t>(g_keys.size())) return 0;
  std::lock_guard lock(g_mutex);
  return g_keys[vk] ? static_cast<std::int16_t>(0x8000) : 0;
}

std::int32_t TrackMouseEvent(Windows::TTrackMouseEvent* event) {
  if (!event || !Handles().IsValid(event->hwndTrack, HandleType::Window)) return 0;
  SetLastError(kErrorSuccess);
  return 1;
}

template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}  // namespace

void InjectHostInput(input_platform::RawInput input) {
  input_platform::InjectHostRaw(&g_input, input);
}
void WarpInputCursor(std::int32_t x, std::int32_t y) {
  std::int32_t width = 1280, height = 720;
  MainWindowSize(&width, &height);
  input_platform::Warp(&g_input, x, y, width, height);
  SetInputCursor(g_input.last.x, g_input.last.y);
}
void PostPaint(std::uint32_t window) { PostMessage(window, kWmPaint, 0, 0); }

ImportAddress ResolveMessageImport(std::string_view dll, std::string_view symbol) {
  if (dll != "user32.dll") return nullptr;
  if (symbol == "PostMessageA") return Address(&PostMessage);
  if (symbol == "PeekMessageA" || symbol == "PeekMessageW") return Address(&PeekMessage);
  if (symbol == "TranslateMessage") return Address(&TranslateMessage);
  if (symbol == "DispatchMessageW") return Address(&DispatchMessage);
  if (symbol == "DefWindowProcW") return Address(&DefWindowProc);
  if (symbol == "PostQuitMessage") return Address(&PostQuit);
  if (symbol == "RegisterWindowMessageA") return Address(&RegisterWindowMessage);
  if (symbol == "SetTimer") return Address(&SetTimer);
  if (symbol == "MsgWaitForMultipleObjects") return Address(&MsgWait);
  if (symbol == "GetAsyncKeyState") return Address(&GetAsyncKeyState);
  if (symbol == "TrackMouseEvent") return Address(&TrackMouseEvent);
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat
