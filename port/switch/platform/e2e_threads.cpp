#include "e2e_threads.hpp"
#if defined(__SWITCH__)
#include "e2e_stage.hpp"
#include <switch/kernel/thread.h>
#include <switch/result.h>
#include <cstdlib>
#endif

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <unordered_map>

namespace srhd_awa::platform::e2e_threads {
namespace {
struct ThreadState {
  std::mutex mutex;
  std::condition_variable changed;
  bool suspended = false;
  bool finished = false;
  bool failed = false;
  bool self_close_requested = false;
  std::int32_t priority = 0;
#if defined(__SWITCH__)
  Thread worker{};
  bool worker_started = false;
#else
  std::thread worker;
#endif
};

#if defined(__SWITCH__)
struct StartArgs {
  std::shared_ptr<ThreadState> state;
  Entry entry;
  void* parameter;
};

void RunThread(void* opaque) {
  std::unique_ptr<StartArgs> args(static_cast<StartArgs*>(opaque));
  auto state = args->state;
  {
    std::unique_lock lock(state->mutex);
    state->changed.wait(lock, [&] { return !state->suspended; });
  }
  try { args->entry(args->parameter); } catch (...) {
    std::lock_guard lock(state->mutex);
    state->failed = true;
    e2e_stage::Log("FAIL stage=worker-thread uncaught-exception");
  }
  {
    std::lock_guard lock(state->mutex);
    state->finished = true;
  }
  state->changed.notify_all();
}
#endif

std::mutex g_mutex;
std::unordered_map<std::uint32_t, std::shared_ptr<ThreadState>> g_threads;
std::uint32_t g_next_handle = 0x50000000u;
}  // namespace

std::uint32_t CurrentId() {
#if defined(__SWITCH__)
  return threadGetCurHandle();
#else
  return static_cast<std::uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id())) | 1u;
#endif
}

std::uint32_t Create(Entry entry, void* parameter, bool suspended,
                     std::uint32_t* thread_id) {
  if (!entry || !thread_id) return 0;
  auto state = std::make_shared<ThreadState>();
  state->suspended = suspended;
  std::lock_guard global_lock(g_mutex);
  if (g_next_handle == 0xffffffffu) return 0;
  const auto handle = g_next_handle++;
  g_threads.emplace(handle, state);
#if defined(__SWITCH__)
  // Preserve the original worker model while bypassing libnx pthread's extra
  // core-mask setup. Retain the raw libnx Result if creation still fails.
  auto* args = new (std::nothrow) StartArgs{state, entry, parameter};
  Result result = args ? threadCreate(&state->worker, RunThread, args, nullptr,
      128 * 1024, 0x2C, -2) : MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
  if (R_SUCCEEDED(result)) {
    result = threadStart(&state->worker);
    if (R_FAILED(result)) threadClose(&state->worker);
  }
  if (R_FAILED(result)) {
    delete args;
    g_threads.erase(handle);
    const std::string detail = "FAIL stage=thread-create libnx_result=" +
        std::to_string(result) + " active=" + std::to_string(g_threads.size()) +
        " stack=131072 priority=44";
    e2e_stage::Log(detail.c_str());
    e2e_stage::LogBootFailed();
    std::_Exit(1);
  }
  state->worker_started = true;
#else
  try {
    state->worker = std::thread([state, entry, parameter] {
      {
        std::unique_lock lock(state->mutex);
        state->changed.wait(lock, [&] { return !state->suspended; });
      }
      try { entry(parameter); } catch (...) {
        std::lock_guard lock(state->mutex);
        state->failed = true;
#if defined(__SWITCH__)
        e2e_stage::Log("FAIL stage=worker-thread uncaught-exception");
#endif
      }
      {
        std::lock_guard lock(state->mutex);
        state->finished = true;
      }
      state->changed.notify_all();
    });
  } catch (...) {
    g_threads.erase(handle);
    return 0;
  }
#endif
#if defined(__SWITCH__)
  *thread_id = state->worker.handle;
#else
  *thread_id = static_cast<std::uint32_t>(std::hash<std::thread::id>{}(state->worker.get_id())) | 1u;
#endif
  return handle;
}

std::uint32_t Resume(std::uint32_t handle) {
  std::shared_ptr<ThreadState> state;
  {
    std::lock_guard lock(g_mutex);
    const auto found = g_threads.find(handle);
    if (found == g_threads.end()) return 0xffffffffu;
    state = found->second;
  }
  std::lock_guard lock(state->mutex);
  const bool was_suspended = state->suspended;
  state->suspended = false;
  state->changed.notify_all();
  return was_suspended ? 1u : 0u;
}

std::uint32_t Wait(std::uint32_t handle, std::uint32_t timeout_ms) {
  std::shared_ptr<ThreadState> state;
  {
    std::lock_guard lock(g_mutex);
    const auto found = g_threads.find(handle);
    if (found == g_threads.end()) return 0xffffffffu;
    state = found->second;
  }
  std::unique_lock lock(state->mutex);
  if (timeout_ms == 0xffffffffu) state->changed.wait(lock, [&] { return state->finished; });
  else if (!state->changed.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                    [&] { return state->finished; })) return 258u;
  const auto result = state->failed ? 0xffffffffu : 0u;
  const bool close_after_wait = state->self_close_requested;
  lock.unlock();
  if (close_after_wait) Close(handle);
  return result;
}

bool Close(std::uint32_t handle) {
  std::shared_ptr<ThreadState> state;
  {
    std::lock_guard lock(g_mutex);
    const auto found = g_threads.find(handle);
    if (found == g_threads.end()) return false;
#if defined(__SWITCH__)
    if (found->second->worker.handle == threadGetCurHandle()) {
#else
    if (found->second->worker.get_id() == std::this_thread::get_id()) {
#endif
      std::lock_guard state_lock(found->second->mutex);
      found->second->self_close_requested = true;
      return true;
    }
    state = std::move(found->second);
    g_threads.erase(found);
  }
#if defined(__SWITCH__)
  if (state->worker_started) {
    std::lock_guard lock(state->mutex);
    if (state->finished) {
      threadWaitForExit(&state->worker);
      threadClose(&state->worker);
    } else {
      // A live worker retains state through StartArgs until its callback ends.
      // The original CloseHandle path is only used after WaitForSingleObject.
      state->self_close_requested = true;
    }
  }
#else
  if (state->worker.joinable()) {
    std::lock_guard lock(state->mutex);
    if (state->finished) state->worker.join();
    else state->worker.detach();
  }
#endif
  return true;
}

bool SetPriority(std::uint32_t handle, std::int32_t priority) {
  std::lock_guard lock(g_mutex);
  const auto found = g_threads.find(handle);
  if (found == g_threads.end()) return handle == CurrentId();
  std::lock_guard state_lock(found->second->mutex);
  found->second->priority = priority;
  return true;
}

std::int32_t GetPriority(std::uint32_t handle) {
  std::lock_guard lock(g_mutex);
  const auto found = g_threads.find(handle);
  if (found == g_threads.end()) return handle == CurrentId() ? 0 : 0x7fffffff;
  std::lock_guard state_lock(found->second->mutex);
  return found->second->priority;
}

}  // namespace srhd_awa::platform::e2e_threads
