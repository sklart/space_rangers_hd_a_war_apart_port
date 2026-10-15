#include "e2e_threads.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace threads = srhd_awa::platform::e2e_threads;

std::int32_t Entry(void* parameter) {
  ++*static_cast<std::atomic<int>*>(parameter);
  return 0;
}

struct SelfCloseState {
  std::uint32_t handle = 0;
  std::atomic<int> closed = 0;
};

std::int32_t EntrySelfClose(void* parameter) {
  auto* state = static_cast<SelfCloseState*>(parameter);
  state->closed = threads::Close(state->handle) ? 1 : -1;
  return 0;
}

int main() {
  std::atomic<int> runs = 0;
  std::uint32_t id = 0;
  const auto handle = threads::Create(Entry, &runs, true, &id);
  if (!handle || !id || id == threads::CurrentId()) return 1;
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  if (runs != 0 || threads::Wait(handle, 0) != 258u) return 2;
  if (!threads::SetPriority(handle, 2) || threads::GetPriority(handle) != 2) return 3;
  if (threads::Resume(handle) != 1u || threads::Wait(handle, 500) != 0u || runs != 1) return 4;
  if (!threads::Close(handle)) return 5;
  SelfCloseState self_close;
  self_close.handle = threads::Create(EntrySelfClose, &self_close, true, &id);
  if (!self_close.handle || threads::Resume(self_close.handle) != 1u ||
      threads::Wait(self_close.handle, 500) != 0u || self_close.closed != 1 ||
      threads::Close(self_close.handle)) return 6;
}
