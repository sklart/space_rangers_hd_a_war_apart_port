#include "e2e_events.hpp"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace srhd_awa::platform::e2e_events {
namespace {
struct Event {
  bool manual_reset;
  bool signaled;
};

std::mutex g_mutex;
std::condition_variable g_changed;
std::unordered_map<std::uint32_t, std::shared_ptr<Event>> g_events;
std::uint32_t g_next_handle = 0x40000000u;
}  // namespace

std::uint32_t Create(bool manual_reset, bool initial_state) {
  std::lock_guard lock(g_mutex);
  if (g_next_handle == 0xffffffffu) return 0;
  const auto handle = g_next_handle++;
  g_events.emplace(handle, std::make_shared<Event>(Event{manual_reset, initial_state}));
  return handle;
}

bool Close(std::uint32_t handle) {
  std::lock_guard lock(g_mutex);
  return g_events.erase(handle) != 0;
}

bool Set(std::uint32_t handle) {
  std::lock_guard lock(g_mutex);
  const auto found = g_events.find(handle);
  if (found == g_events.end()) return false;
  found->second->signaled = true;
  g_changed.notify_all();
  return true;
}

bool Reset(std::uint32_t handle) {
  std::lock_guard lock(g_mutex);
  const auto found = g_events.find(handle);
  if (found == g_events.end()) return false;
  found->second->signaled = false;
  return true;
}

std::uint32_t WaitOne(std::uint32_t handle, std::uint32_t timeout_ms) {
  return WaitMany(&handle, 1, false, timeout_ms);
}

std::uint32_t WaitMany(const std::uint32_t* handles, std::uint32_t count,
                       bool wait_all, std::uint32_t timeout_ms) {
  if (!handles || count == 0 || count > 64) return kWaitFailed;
  std::unique_lock lock(g_mutex);
  std::vector<std::shared_ptr<Event>> events;
  events.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    const auto found = g_events.find(handles[index]);
    if (found == g_events.end()) return kWaitFailed;
    events.push_back(found->second);
  }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms == kInfinite ? 0 : timeout_ms);
  while (true) {
    if (wait_all) {
      bool ready = true;
      for (const auto& event : events) ready &= event->signaled;
      if (ready) {
        for (const auto& event : events) if (!event->manual_reset) event->signaled = false;
        return kWaitObject0;
      }
    } else {
      for (std::uint32_t index = 0; index < count; ++index) {
        if (events[index]->signaled) {
          if (!events[index]->manual_reset) events[index]->signaled = false;
          return kWaitObject0 + index;
        }
      }
    }
    if (timeout_ms == 0) return kWaitTimeout;
    if (timeout_ms == kInfinite) g_changed.wait(lock);
    else if (g_changed.wait_until(lock, deadline) == std::cv_status::timeout) return kWaitTimeout;
  }
}

}  // namespace srhd_awa::platform::e2e_events
