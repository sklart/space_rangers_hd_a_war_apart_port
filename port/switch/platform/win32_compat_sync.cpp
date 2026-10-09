#include "win32_compat_sync.hpp"

#include "e2e_events.hpp"
#include "e2e_stage.hpp"
#include "e2e_threads.hpp"
#include "win32_handles.hpp"
#include "win32_compat_files.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace srhd_awa::platform::win32_compat {
namespace {
constexpr std::uint32_t kCurrentThreadPseudoHandle = 0xfffffffeu;

std::uint32_t CreateEventThunk(void*, std::int32_t manual_reset,
                               std::int32_t initial_state, std::uint8_t*) {
  const auto backend = e2e_events::Create(manual_reset != 0, initial_state != 0);
  if (!backend) return 0;
  const auto token = Handles().Allocate(HandleType::Event,
                                         std::make_shared<std::uint32_t>(backend));
  if (!token) e2e_events::Close(backend);
  return token;
}

std::uint32_t CreateThreadThunk(void*, std::uint32_t,
                                e2e_threads::Entry entry, void* parameter,
                                std::uint32_t flags, std::uint32_t* thread_id) {
  if (!thread_id) { SetLastError(kErrorInvalidParameter); return 0; }
  const auto backend = e2e_threads::Create(entry, parameter, (flags & 4u) != 0,
                                            thread_id);
  if (!backend) return 0;
  const auto token = Handles().Allocate(HandleType::Thread,
                                         std::make_shared<std::uint32_t>(backend));
  if (!token) e2e_threads::Close(backend);
  return token;
}

std::uint32_t Backend(std::uint32_t token, HandleType type,
                      bool probe = false) {
  const auto object = probe ? Handles().TryLookup(token, type)
                            : Handles().Lookup(token, type);
  return object ? *static_cast<std::uint32_t*>(object.get()) : 0;
}

std::int32_t SetEventThunk(std::uint32_t token) {
  const auto backend = Backend(token, HandleType::Event);
  return backend && e2e_events::Set(backend) ? 1 : 0;
}

std::int32_t ResetEventThunk(std::uint32_t token) {
  const auto backend = Backend(token, HandleType::Event);
  return backend && e2e_events::Reset(backend) ? 1 : 0;
}

std::uint32_t WaitForSingleObjectThunk(std::uint32_t token,
                                       std::uint32_t timeout) {
  if (const auto event = Backend(token, HandleType::Event, true))
    return e2e_events::WaitOne(event, timeout);
  if (const auto thread = Backend(token, HandleType::Thread, true))
    return e2e_threads::Wait(thread, timeout);
  Handles().Lookup(token, HandleType::Event); // One diagnostic for a truly invalid token.
  return e2e_events::kWaitFailed;
}

std::uint32_t WaitForMultipleObjectsThunk(std::uint32_t count,
                                          const std::uint32_t* tokens,
                                          std::int32_t wait_all,
                                          std::uint32_t timeout) {
  if (!tokens || !count || count > 64) {
    SetLastError(kErrorInvalidParameter);
    return e2e_events::kWaitFailed;
  }
  std::vector<std::uint32_t> backend;
  backend.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    const auto event = Backend(tokens[i], HandleType::Event);
    if (!event) { SetLastError(kErrorInvalidHandle); return e2e_events::kWaitFailed; }
    backend.push_back(event);
  }
  return e2e_events::WaitMany(backend.data(), count, wait_all != 0, timeout);
}

std::int32_t CloseHandleThunk(std::uint32_t token) {
  if (Handles().TryLookup(token, HandleType::File))
    return CloseFileHandle(token) ? 1 : 0;
  if (const auto event = Backend(token, HandleType::Event, true)) {
    if (!Handles().Close(token, HandleType::Event)) return 0;
    return e2e_events::Close(event) ? 1 : 0;
  }
  if (const auto thread = Backend(token, HandleType::Thread, true)) {
    if (!Handles().Close(token, HandleType::Thread)) return 0;
    return e2e_threads::Close(thread) ? 1 : 0;
  }
  SetLastError(kErrorInvalidHandle);
  e2e_stage::LogWinApi("INVALID HANDLE operation=CloseHandle");
  return 0;
}

std::uint32_t ResumeThreadThunk(std::uint32_t token) {
  const auto backend = Backend(token, HandleType::Thread);
  return backend ? e2e_threads::Resume(backend) : 0xffffffffu;
}

std::uint32_t GetCurrentThreadThunk() { return kCurrentThreadPseudoHandle; }
std::uint32_t GetCurrentThreadIdThunk() { return e2e_threads::CurrentId(); }

std::int32_t SetThreadPriorityThunk(std::uint32_t token, std::int32_t priority) {
  const auto backend = token == kCurrentThreadPseudoHandle
      ? e2e_threads::CurrentId() : Backend(token, HandleType::Thread);
  return backend && e2e_threads::SetPriority(backend, priority) ? 1 : 0;
}

std::int32_t GetThreadPriorityThunk(std::uint32_t token) {
  const auto backend = token == kCurrentThreadPseudoHandle
      ? e2e_threads::CurrentId() : Backend(token, HandleType::Thread);
  return backend ? e2e_threads::GetPriority(backend) : 0x7fffffff;
}

std::uint32_t OpenEventThunk(std::uint32_t, std::int32_t, std::uint8_t*) {
  SetLastError(kErrorFileNotFound);
  return 0; // Single-instance named event is absent in the Switch process.
}

std::int32_t TerminateThreadThunk(std::uint32_t token, std::uint32_t) {
  if (Backend(token, HandleType::Thread)) {
    SetLastError(kErrorAccessDenied);
    e2e_stage::LogWinApi("UNSUPPORTED operation=TerminateThread live-thread");
  } else {
    SetLastError(kErrorInvalidHandle);
  }
  return 0;
}

template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}  // namespace

ImportAddress ResolveSyncImport(std::string_view dll, std::string_view symbol) {
  if (dll != "kernel32.dll") return nullptr;
  if (symbol == "CreateEventA") return Address(&CreateEventThunk);
  if (symbol == "CreateThread") return Address(&CreateThreadThunk);
  if (symbol == "SetEvent") return Address(&SetEventThunk);
  if (symbol == "ResetEvent") return Address(&ResetEventThunk);
  if (symbol == "WaitForSingleObject") return Address(&WaitForSingleObjectThunk);
  if (symbol == "WaitForMultipleObjects") return Address(&WaitForMultipleObjectsThunk);
  if (symbol == "CloseHandle") return Address(&CloseHandleThunk);
  if (symbol == "ResumeThread") return Address(&ResumeThreadThunk);
  if (symbol == "GetCurrentThread") return Address(&GetCurrentThreadThunk);
  if (symbol == "GetCurrentThreadId") return Address(&GetCurrentThreadIdThunk);
  if (symbol == "SetThreadPriority") return Address(&SetThreadPriorityThunk);
  if (symbol == "GetThreadPriority") return Address(&GetThreadPriorityThunk);
  if (symbol == "OpenEventA") return Address(&OpenEventThunk);
  if (symbol == "TerminateThread") return Address(&TerminateThreadThunk);
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat
