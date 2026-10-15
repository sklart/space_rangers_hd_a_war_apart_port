#include "win32_handles.hpp"

#include "e2e_stage.hpp"

#include <mutex>
#include <string>
#include <unordered_map>

namespace srhd_awa::platform::win32_compat {
namespace {
struct Entry {
  HandleType type;
  std::shared_ptr<void> object;
};
std::mutex g_mutex;
std::unordered_map<std::uint32_t, Entry> g_entries;
std::uint32_t g_next_handle = 0x60000000u;
thread_local std::uint32_t g_last_error = kErrorSuccess;
WinHandleTable g_table;
void LogInvalid(const char* operation, std::uint32_t handle, HandleType type) {
  const auto detail = std::string("INVALID HANDLE operation=") + operation +
                      " token=" + std::to_string(handle) +
                      " type=" + std::to_string(static_cast<unsigned>(type)) +
                      " stage=" + e2e_stage::CurrentStage();
  e2e_stage::LogWinApi(detail.c_str());
}
}  // namespace

void SetLastError(std::uint32_t error) { g_last_error = error; }
std::uint32_t GetLastError() { return g_last_error; }

std::uint32_t WinHandleTable::Allocate(HandleType type,
                                       std::shared_ptr<void> object) {
  if (!object) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  std::lock_guard lock(g_mutex);
  if (g_next_handle == 0xffffffffu) {
    SetLastError(kErrorInvalidHandle);
    return 0;
  }
  const auto handle = g_next_handle++;
  g_entries.emplace(handle, Entry{type, std::move(object)});
  SetLastError(kErrorSuccess);
  return handle;
}

std::shared_ptr<void> WinHandleTable::Lookup(std::uint32_t handle,
                                              HandleType type) {
  std::lock_guard lock(g_mutex);
  const auto found = g_entries.find(handle);
  if (found == g_entries.end() || found->second.type != type) {
    SetLastError(kErrorInvalidHandle);
    LogInvalid("lookup", handle, type);
    return {};
  }
  SetLastError(kErrorSuccess);
  return found->second.object;
}

std::shared_ptr<void> WinHandleTable::TryLookup(std::uint32_t handle,
                                                 HandleType type) {
  std::lock_guard lock(g_mutex);
  const auto found = g_entries.find(handle);
  return found != g_entries.end() && found->second.type == type
      ? found->second.object : std::shared_ptr<void>{};
}

bool WinHandleTable::Close(std::uint32_t handle, HandleType type) {
  std::lock_guard lock(g_mutex);
  const auto found = g_entries.find(handle);
  if (found == g_entries.end() || found->second.type != type) {
    SetLastError(kErrorInvalidHandle);
    LogInvalid("close", handle, type);
    return false;
  }
  g_entries.erase(found);
  SetLastError(kErrorSuccess);
  return true;
}

bool WinHandleTable::IsValid(std::uint32_t handle, HandleType type) {
  return static_cast<bool>(Lookup(handle, type));
}

WinHandleTable& Handles() { return g_table; }
}  // namespace srhd_awa::platform::win32_compat
