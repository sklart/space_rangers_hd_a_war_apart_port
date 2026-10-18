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
constexpr std::uint32_t kVkF5 = 0x74u;
constexpr std::uint32_t kVkLeftMouse = 1u;
constexpr std::uint32_t kVkRightMouse = 2u;
constexpr std::uint32_t kWaitFailed = 0xffffffffu;
constexpr std::uint32_t kWaitTimeout = 258u;
struct Timer {
  std::uint32_t window = 0, id = 0, interval = 0;
  std::uint64_t due = 0;
};
struct QueuedMessage {
  Windows::TMsg message{};
  std::uint64_t press_id{};
  std::uint64_t hid_sequence{};
  bool touch{};
};
std::mutex g_mutex;
std::deque<QueuedMessage> g_queue;
std::uint64_t g_next_press_id{};
std::uint64_t g_active_press_id{};
thread_local std::uint64_t g_dispatched_press_id{};
thread_local bool g_dispatched_touch{};
std::unordered_map<std::string, std::uint32_t> g_registered;
std::uint32_t g_next_registered = 0xc000u;
std::vector<std::shared_ptr<Timer>> g_timers;
input_platform::State g_input;
#if !defined(__SWITCH__)
// The host driver runs on another thread. Publish only raw snapshots here;
// the UI thread alone owns the normalizer and its cursor/edge state.
std::mutex g_host_input_mutex;
input_platform::RawInput g_host_input;
#endif
std::array<bool, 256> g_keys{};
std::int32_t g_last_x = -1, g_last_y = -1;
#if defined(__SWITCH__)
std::uint32_t g_last_input_diagnostic_tick{};
bool g_input_diagnostic_started{};
std::uint64_t g_poll_window_start_us{};
std::uint64_t g_previous_poll_us{};
std::uint64_t g_poll_count{};
std::uint64_t g_pointer_move_count{};
std::uint64_t g_max_poll_gap_us{};
#endif

std::int32_t MouseLParam(std::int32_t x, std::int32_t y) {
  return static_cast<std::int32_t>((static_cast<std::uint32_t>(y) & 0xffffu) << 16 |
                                    (static_cast<std::uint32_t>(x) & 0xffffu));
}

void Enqueue(std::uint32_t window, std::uint32_t message,
             std::uint32_t wparam = 0, std::int32_t lparam = 0,
             std::int32_t x = 0, std::int32_t y = 0,
             std::uint64_t hid_sequence = 0, bool touch = false) {
  Windows::TMsg item{};
  item.hwnd = window;
  item.message = message;
  item.wParam = wparam;
  item.lParam = lparam;
  item.time = e2e_clock::Milliseconds();
  item.pt = {x, y};
  std::uint64_t press_id{};
  if (message == kWmLeftDown) {
    press_id = ++g_next_press_id;
    g_active_press_id = press_id;
  } else if (message == kWmLeftUp) {
    press_id = g_active_press_id;
    g_active_press_id = 0;
  }
  g_queue.push_back({item, press_id, hid_sequence, touch});
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
#if !defined(__SWITCH__)
  {
    std::lock_guard lock(g_host_input_mutex);
    input_platform::InjectHostRaw(&g_input, g_host_input);
  }
#endif
  const auto batch = input_platform::PollHistory(&g_input, width, height);
  if (!batch.count) return;
  const auto& input = batch.snapshots[batch.count - 1];
  SetInputCursor(input.x, input.y);
#if defined(__SWITCH__)
  const auto poll_us = e2e_clock::Microseconds();
  if (g_poll_window_start_us == 0) g_poll_window_start_us = poll_us;
  if (g_previous_poll_us && poll_us >= g_previous_poll_us)
    g_max_poll_gap_us = std::max(g_max_poll_gap_us, poll_us - g_previous_poll_us);
  g_previous_poll_us = poll_us;
  ++g_poll_count;
  if (input.x != g_last_x || input.y != g_last_y) ++g_pointer_move_count;
  const auto window_us = poll_us - g_poll_window_start_us;
  if (window_us >= 5000000u) {
    char rate_detail[160]{};
    std::snprintf(rate_detail, sizeof(rate_detail),
        "input cadence polls=%llu moves=%llu window_us=%llu max_gap_us=%llu",
        static_cast<unsigned long long>(g_poll_count),
        static_cast<unsigned long long>(g_pointer_move_count),
        static_cast<unsigned long long>(window_us),
        static_cast<unsigned long long>(g_max_poll_gap_us));
    e2e_stage::Log(rate_detail);
    g_poll_window_start_us = poll_us;
    g_poll_count = g_pointer_move_count = g_max_poll_gap_us = 0;
  }
  const auto diagnostic_tick = e2e_clock::Milliseconds();
  const bool button_edge = std::any_of(batch.snapshots.begin(),
      batch.snapshots.begin() + batch.count, [](const auto& item) {
        return item.left_down || item.left_up || item.right_down ||
            item.right_up || item.enter_down || item.enter_up ||
            item.escape_down || item.escape_up || item.plus_down ||
            item.quicksave_down || item.quicksave_up;
      });
  if (!g_input_diagnostic_started || button_edge ||
      diagnostic_tick - g_last_input_diagnostic_tick >= 1000u) {
    char detail[192]{};
    std::snprintf(detail, sizeof(detail),
        "input poll window=%u size=%dx%d cursor=%d,%d buttons=A%d B%d X%d Y%d PLUS%d samples=%zu missed=%llu",
        window, width, height, input.x, input.y,
        input.left_held, input.right_held, input.enter_held,
        input.escape_held, input.plus_down, batch.count,
        static_cast<unsigned long long>(batch.missed_samples));
    e2e_stage::Log(detail);
    g_last_input_diagnostic_tick = diagnostic_tick;
    g_input_diagnostic_started = true;
  }
#endif
  std::lock_guard lock(g_mutex);
  for (std::size_t index = 0; index < batch.count; ++index) {
    const auto& sample = batch.snapshots[index];
    const auto lparam = MouseLParam(sample.x, sample.y);
    const bool pointer_target = index + 1 == batch.count || sample.left_down ||
        sample.left_up || sample.right_down || sample.right_up;
    if (pointer_target && (sample.x != g_last_x || sample.y != g_last_y)) {
      Enqueue(window, kWmMouseMove, 0, lparam, sample.x, sample.y,
              sample.hid_sequence, sample.touch_message);
      g_last_x = sample.x; g_last_y = sample.y;
    }
    if (sample.left_down) Enqueue(window, kWmLeftDown, 1, lparam, sample.x, sample.y,
                                  sample.hid_sequence, sample.touch_message);
    if (sample.left_up) Enqueue(window, kWmLeftUp, 0, lparam, sample.x, sample.y,
                                sample.hid_sequence, sample.touch_message);
    if (sample.right_down) Enqueue(window, kWmRightDown, 2, lparam, sample.x, sample.y);
    if (sample.right_up) Enqueue(window, kWmRightUp, 0, lparam, sample.x, sample.y);
    auto key = [&](std::uint32_t vk, bool down, bool up, bool held) {
      g_keys[vk] = held;
      if (down) Enqueue(window, kWmKeyDown, vk);
      if (up) Enqueue(window, kWmKeyUp, vk);
    };
    key(kVkReturn, sample.enter_down, sample.enter_up, sample.enter_held);
    key(kVkEscape, sample.escape_down, sample.escape_up, sample.escape_held);
    key(kVkF5, sample.quicksave_down, sample.quicksave_up, sample.quicksave_held);
    constexpr std::uint32_t flight_vks[]{0x26, 0x28, 0x25, 0x27, 0x11, 0x20};
    for (unsigned bit = 0; bit < 6; ++bit) {
      const auto vk = flight_vks[bit];
      const bool held = (sample.flight_keys & (1u << bit)) != 0;
      const bool previous = g_keys[vk];
      key(vk, held && !previous, !held && previous, held);
    }
    g_keys[kVkLeftMouse] = sample.left_held;
    g_keys[kVkRightMouse] = sample.right_held;
    if (sample.plus_down) Enqueue(0, kWmQuit); // PLUS is process exit, never a VK.
  }
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
  std::unique_lock lock(g_mutex);
  auto found = std::find_if(g_queue.begin(), g_queue.end(), [&](const auto& item) {
    const bool hwnd_match = !window || item.message.hwnd == window ||
        item.message.message == kWmQuit;
    const bool filter_match = (!first && !last) ||
        (item.message.message >= first && item.message.message <= last) ||
        item.message.message == kWmQuit;
    return hwnd_match && filter_match;
  });
  if (found == g_queue.end()) return 0;
  *output = found->message;
  const auto press_id = found->press_id;
  const auto hid_sequence = found->hid_sequence;
  if (remove & 1u) {
    g_dispatched_press_id = found->press_id;
    g_dispatched_touch = found->touch;
    if (output->message == kWmLeftDown) g_keys[kVkLeftMouse] = true;
    if (output->message == kWmLeftUp) g_keys[kVkLeftMouse] = false;
    if (output->message == kWmRightDown) g_keys[kVkRightMouse] = true;
    if (output->message == kWmRightUp) g_keys[kVkRightMouse] = false;
    if ((output->message == kWmKeyDown || output->message == kWmKeyUp) &&
        output->wParam < g_keys.size())
      g_keys[output->wParam] = output->message == kWmKeyDown;
    g_queue.erase(found);
  }
  SetLastError(kErrorSuccess);
  lock.unlock();
  if ((remove & 1u) && press_id &&
      (output->message == kWmLeftDown || output->message == kWmLeftUp)) {
    char detail[160]{};
    std::snprintf(detail, sizeof(detail),
        "input press_id=%llu phase=dispatch message=%u hid_sequence=%llu queued_ms=%u dispatch_ms=%u x=%d y=%d",
        static_cast<unsigned long long>(press_id), output->message,
        static_cast<unsigned long long>(hid_sequence), output->time,
        e2e_clock::Milliseconds(), static_cast<std::int16_t>(output->lParam),
        static_cast<std::int16_t>(output->lParam >> 16));
    e2e_stage::Log(detail);
  }
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
#if !defined(__SWITCH__)
  std::lock_guard lock(g_host_input_mutex);
  g_host_input = input;
#else
  static_cast<void>(input); // Physical input is sampled by the UI thread.
#endif
}
void WarpInputCursor(std::int32_t x, std::int32_t y) {
  std::int32_t width = 1280, height = 720;
  MainWindowSize(&width, &height);
  input_platform::Warp(&g_input, x, y, width, height);
  SetInputCursor(g_input.last.x, g_input.last.y);
}
void PostPaint(std::uint32_t window) { PostMessage(window, kWmPaint, 0, 0); }
std::uint64_t CurrentPressId() { return g_dispatched_press_id; }
bool CurrentMessageIsTouch() { return g_dispatched_touch; }
#if defined(E2E5_HOST_SDL_WINDOW)
bool PostHostKey(std::uint32_t window, std::uint32_t virtual_key) {
  if (!window) return false;
  return PostMessage(window, kWmKeyDown, virtual_key, 0) &&
         PostMessage(window, kWmKeyUp, virtual_key, 0);
}
bool PostHostReturnKey(std::uint32_t window) { return PostHostKey(window, kVkReturn); }
bool PostHostLeftClick(std::uint32_t window, std::int32_t x, std::int32_t y) {
  if (!window || !Handles().IsValid(window, HandleType::Window) ||
      x < 0 || y < 0 || x >= 65536 || y >= 65536) return false;
  const auto lparam = MouseLParam(x, y);
  std::lock_guard lock(g_mutex);
  Enqueue(window, kWmMouseMove, 0, lparam, x, y);
  Enqueue(window, kWmLeftDown, 1, lparam, x, y);
  Enqueue(window, kWmLeftUp, 0, lparam, x, y);
  return true;
}
#endif

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
