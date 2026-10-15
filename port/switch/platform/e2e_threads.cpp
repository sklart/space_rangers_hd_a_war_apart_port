#include "e2e_threads.hpp"
#if defined(__SWITCH__)
#include "e2e_stage.hpp"
#include <switch.h>
#include <cstdlib>
#endif

#include <algorithm>
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

bool PreferCalculationCore(std::uint32_t handle) {
  std::lock_guard lock(g_mutex);
  const auto found = g_threads.find(handle);
  if (found == g_threads.end()) return false;
#if defined(__SWITCH__)
  std::uint64_t allowed = 0;
  const Result info = svcGetInfo(&allowed, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0);
  const auto ui_core = svcGetCurrentProcessorNumber();
  int target = -1;
  if (R_SUCCEEDED(info)) {
    for (int core = 0; core < 3; ++core)
      if (core != ui_core && (allowed & (1ull << core))) { target = core; break; }
  }
  const Result set = target < 0 ? info :
      svcSetThreadCoreMask(found->second->worker.handle, target, 1u << target);
  std::int32_t actual_core = -1;
  std::uint64_t actual_mask = 0;
  const Result query = svcGetThreadCoreMask(&actual_core, &actual_mask, found->second->worker.handle);
  char detail[256]{};
  std::snprintf(detail, sizeof(detail),
      "calculation affinity ui_core=%d requested=%d allowed=%llx actual=%d mask=%llx info_result=%u set_result=%u query_result=%u applied=%d",
      ui_core, target, static_cast<unsigned long long>(allowed), actual_core,
      static_cast<unsigned long long>(actual_mask), info, set, query,
      target >= 0 && R_SUCCEEDED(set) && R_SUCCEEDED(query));
  e2e_stage::Log(detail);
  return target >= 0 && R_SUCCEEDED(set) && R_SUCCEEDED(query);
#else
  return true; // Host scheduling already permits separate CPU cores.
#endif
}

bool SetPriority(std::uint32_t handle, std::int32_t priority) {
  std::lock_guard lock(g_mutex);
  const auto found = g_threads.find(handle);
  if (found == g_threads.end()) return handle == CurrentId();
  std::lock_guard state_lock(found->second->mutex);
#if defined(__SWITCH__)
  // The original music worker requests Win32 ABOVE_NORMAL (+1). Its libnx
  // thread otherwise stays at the default 0x2C throughout long render work.
  // Switch uses lower numeric values for higher scheduling priority.
  const auto switch_priority = static_cast<std::uint32_t>(
      0x2C - std::clamp(priority, -2, 2));
  const Result result = svcSetThreadPriority(found->second->worker.handle,
                                              switch_priority);
  LogPriority("worker", found->second->worker.handle, switch_priority, result);
  if (R_FAILED(result)) {
    const std::string detail = "FAIL stage=thread-priority libnx_result=" +
        std::to_string(result) + " requested=" + std::to_string(priority);
    e2e_stage::Log(detail.c_str());
    return false;
  }
#endif
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
