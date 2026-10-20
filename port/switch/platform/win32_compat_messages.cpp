#include "win32_compat_messages.hpp"

#include "e2e_clock.hpp"
#include "controller_messages.hpp"
#include "e2e_events.hpp"
#include "e2e_stage.hpp"
#include "win32_compat_window.hpp"
#include "win32_handles.hpp"
#include "types/Windows_group.hpp"

#include <algorithm>
#include <array>
#include <atomic>
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
constexpr std::uint32_t kVkPause = 0x13u;
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
std::uint64_t g_host_input_revision{};
std::atomic<std::uint64_t> g_host_consumed_revision{};
#endif
std::array<bool, 256> g_keys{};
bool g_emitted_escape{}, g_emitted_pause{};
thread_local InputContextScope* g_input_scope{};
bool g_controller_connected{true};
input_platform::ControllerStyle g_controller_style{input_platform::ControllerStyle::Host};
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
  std::uint64_t host_revision{};
  {
    std::lock_guard lock(g_host_input_mutex);
    input_platform::InjectHostRaw(&g_input, g_host_input);
    host_revision = g_host_input_revision;
  }
#endif
  auto batch = input_platform::PollHistory(&g_input, width, height);
#if !defined(__SWITCH__)
  g_host_consumed_revision.store(host_revision, std::memory_order_release);
#endif
  if (!batch.count) return;
  const auto& input = batch.snapshots[batch.count - 1];
  const bool source_changed = g_controller_connected &&
      (!input.connected || input.controller_style != g_controller_style);
  if (source_changed && g_input_scope) g_input_scope->CancelHeldInput();
  g_controller_connected = input.connected;
  g_controller_style = input.controller_style;
  if (source_changed || !input.connected) {
    // Cancel on the old owner, never turn a disconnect into a UI click-up.
    for (std::size_t i = 0; i < batch.count; ++i) {
      auto& sample = batch.snapshots[i];
      const auto x = sample.x, y = sample.y;
      const auto style = sample.controller_style;
      const auto connected = sample.connected;
      const auto actions = sample.actions;
      sample = {};
      sample.x = x; sample.y = y; sample.controller_style = style;
      sample.connected = connected; sample.actions = actions;
    }
  }
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
    if (sample.ui_mode_changed)
      Enqueue(window, controller_messages::kMessage,
          static_cast<unsigned>(controller_actions::Action::ToggleNavigationMode),
          static_cast<int>(sample.ui_focus ? controller_messages::Edge::FocusMode : controller_messages::Edge::PointerMode));
    for (const auto& edge : {std::pair{sample.actions.pressed, controller_messages::Edge::Pressed},
                            std::pair{sample.actions.released, controller_messages::Edge::Released},
                            std::pair{sample.actions.repeat, controller_messages::Edge::Repeat}}) {
      for (const auto action : {controller_actions::Action::Confirm, controller_actions::Action::Cancel,
          controller_actions::Action::NavigateUp, controller_actions::Action::NavigateDown,
          controller_actions::Action::NavigateLeft, controller_actions::Action::NavigateRight,
          controller_actions::Action::Inspect, controller_actions::Action::OpenContextAction,
          controller_actions::Action::PrevTab, controller_actions::Action::NextTab,
          controller_actions::Action::OpenControllerSettings}) {
        if ((sample.ui_focus || action == controller_actions::Action::OpenControllerSettings) &&
            (edge.first & controller_actions::Bit(action)))
          Enqueue(window, controller_messages::kMessage, static_cast<unsigned>(action), static_cast<int>(edge.second));
      }
    }
    for (const auto& edge : {std::pair{sample.actions.pressed, "pressed"},
                            std::pair{sample.actions.released, "released"},
                            std::pair{sample.actions.repeat, "repeat"}}) {
      for (unsigned i = 0; i < static_cast<unsigned>(controller_actions::Action::Count); ++i) {
        if (!(edge.first & (std::uint64_t{1} << i))) continue;
        char detail[224]{};
        std::snprintf(detail, sizeof(detail),
            "input action time_us=%llu source=%s style=%u context=%u action=%s edge=%s target=legacy-adapter window=%u",
            static_cast<unsigned long long>(e2e_clock::Microseconds()),
            sample.touch_message ? "touch" : "pad", static_cast<unsigned>(sample.controller_style),
            static_cast<unsigned>(sample.actions.context),
            controller_actions::Name(static_cast<controller_actions::Action>(i)), edge.second, window);
        e2e_stage::Log(detail);
      }
    }
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
    if (sample.left_up && !sample.ui_mode_changed) Enqueue(window, kWmLeftUp, 0, lparam, sample.x, sample.y,
                                sample.hid_sequence, sample.touch_message);
    if (sample.right_down) Enqueue(window, kWmRightDown, 2, lparam, sample.x, sample.y);
    if (sample.right_up && !sample.ui_mode_changed) Enqueue(window, kWmRightUp, 0, lparam, sample.x, sample.y);
    auto key = [&](std::uint32_t vk, bool down, bool up, bool held) {
      g_keys[vk] = held;
      if (down) Enqueue(window, kWmKeyDown, vk);
      if (up) Enqueue(window, kWmKeyUp, vk);
    };
    key(kVkReturn, sample.enter_down, sample.enter_up, sample.enter_held);
    // Transitional adapters to existing handlers. PLUS never posts WM_QUIT.
    // Arcade has an actual Pause handler; Escape there asks to leave the arena.
    const auto context = sample.actions.context;
    const bool paused = context == controller_actions::Context::ArcadePause;
    const bool arcade = context == controller_actions::Context::ArcadeGameplay || paused;
    const bool escape = (sample.escape_held && !paused) ||
        (sample.pause_held && !arcade && context != controller_actions::Context::MainMenu);
    const bool pause = (sample.pause_held && arcade) || (sample.escape_held && paused);
    key(kVkEscape, escape && !g_emitted_escape, !escape && g_emitted_escape, escape);
    key(kVkPause, pause && !g_emitted_pause, !pause && g_emitted_pause, pause);
    g_emitted_escape = escape; g_emitted_pause = pause;
#if !defined(__SWITCH__) || (defined(E2E4_QUICKSAVE_EXPERIMENT) && E2E4_QUICKSAVE_EXPERIMENT)
    key(kVkF5, sample.quicksave_down, sample.quicksave_up, sample.quicksave_held);
#endif
    constexpr std::uint32_t flight_vks[]{0x26, 0x28, 0x25, 0x27, 0x11, 0x20, 0xdb, 0xdd, 'A'};
    for (unsigned bit = 0; bit < 9; ++bit) {
      const auto vk = flight_vks[bit];
      const bool held = (sample.flight_keys & (1u << bit)) != 0 ||
          (bit == 5 && sample.result_confirm_held);
      const bool previous = g_keys[vk];
      key(vk, held && !previous, !held && previous, held);
    }
    g_keys[kVkLeftMouse] = sample.left_held;
    g_keys[kVkRightMouse] = sample.right_held;
  }
  const auto now = static_cast<std::uint64_t>(e2e_clock::Milliseconds());
  for (const auto& timer : g_timers) {
    if (now >= timer->due) {
      Enqueue(timer->window, kWmTimer, timer->id);
      timer->due = now + timer->interval;
    }
  }
}

namespace {
ReleasedInput ChangeInputOwner(controller_actions::Context context) {
  std::lock_guard lock(g_mutex);
  ReleasedInput result{g_keys};
  for (const auto& queued : g_queue) {
    if (queued.message.message == kWmKeyUp && queued.message.wParam < result.keys.size())
      result.keys[queued.message.wParam] = true;
    if (queued.message.message == kWmLeftUp) result.keys[kVkLeftMouse] = true;
    if (queued.message.message == kWmRightUp) result.keys[kVkRightMouse] = true;
  }
  std::erase_if(g_queue, [](const auto& queued) {
    const auto message = queued.message.message;
    return (message >= 0x100 && message <= 0x109) || (message >= 0x200 && message <= 0x20e) ||
        message == controller_messages::kMessage;
  });
  g_keys.fill(false);
  g_emitted_escape = g_emitted_pause = false;
  g_active_press_id = 0;
  g_last_x = g_last_y = -1;
  // Keep cursor/time and router's old holds, so its next Update blocks the
  // still-held physical inputs and emits logical cancellations.
  g_input.last.left_held = g_input.last.right_held = false;
  g_input.last.enter_held = g_input.last.escape_held = false;
  g_input.last.pause_held = g_input.last.quicksave_held = false;
  g_input.plus_held = false;
  g_input.last.flight_keys = 0;
  input_platform::SetInputContext(context);
  return result;
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

InputContextScope::InputContextScope(controller_actions::Context context, void* owner,
                                     ReleaseInputOwner release)
    : previous_(g_input_scope), context_(context), owner_(owner), release_(release) {
  const auto held = ChangeInputOwner(context_);
  g_input_scope = this;
  if (previous_ && previous_->release_) previous_->release_(previous_->owner_, held);
}
InputContextScope::~InputContextScope() {
  const auto held = ChangeInputOwner(previous_ ? previous_->context_ : controller_actions::Context::MainMenu);
  g_input_scope = previous_;
  if (release_) release_(owner_, held);
}
void InputContextScope::CancelHeldInput() {
  const auto held = ChangeInputOwner(context_);
  if (release_) release_(owner_, held);
}
bool InputContextScope::UpdateContext(void* owner, controller_actions::Context context) {
  if (owner != owner_ || context == context_) return false;
  context_ = context;
  CancelHeldInput();
  return true;
}
bool UpdateActiveInputContext(void* owner, controller_actions::Context context) {
  return g_input_scope && g_input_scope->UpdateContext(owner, context);
}
void InputContextScope::CancelPointerForMode(void* owner) {
  if (owner != owner_) return;
  ReleasedInput held{};
  {
    std::lock_guard lock(g_mutex);
    held.keys[1] = g_keys[1]; held.keys[2] = g_keys[2];
    g_keys[1] = g_keys[2] = false;
    g_active_press_id = 0;
    g_input.last.left_held = g_input.last.right_held = false;
  }
  // Keep completed HID pulses and new navigation packets in their original
  // order. A mode switch cancels a hold, not earlier completed presses.
  if (release_) release_(owner_, held);
}
void CancelActivePointerForMode(void* owner) {
  if (g_input_scope) g_input_scope->CancelPointerForMode(owner);
}

void InjectHostInput(input_platform::RawInput input) {
  static_cast<void>(InjectHostInputTracked(input));
}
std::uint64_t InjectHostInputTracked(input_platform::RawInput input) {
#if !defined(__SWITCH__)
  std::lock_guard lock(g_host_input_mutex);
  g_host_input = input;
  return ++g_host_input_revision;
#else
  static_cast<void>(input); // Physical input is sampled by the UI thread.
  return 0;
#endif
}
bool HostInputConsumed(std::uint64_t revision) {
#if !defined(__SWITCH__)
  return revision != 0 && g_host_consumed_revision.load(std::memory_order_acquire) >= revision;
#else
  static_cast<void>(revision);
  return false;
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
bool PostHostText(std::uint32_t window, std::u16string_view text) {
  if (!window) return false;
  for (const auto character : text)
    if (!PostMessage(window, kWmChar, character, 0)) return false;
  return true;
}
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
