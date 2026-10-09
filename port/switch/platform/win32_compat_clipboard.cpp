#include "win32_compat_clipboard.hpp"

#include "win32_handles.hpp"

#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace srhd_awa::platform::win32_compat {
namespace {
std::mutex g_mutex;
std::thread::id g_owner;
std::unordered_map<std::uint32_t, std::uint32_t> g_formats;

std::int32_t OpenClipboard(std::uint32_t) {
  std::lock_guard lock(g_mutex);
  if (g_owner != std::thread::id{}) { SetLastError(kErrorAccessDenied); return 0; }
  g_owner = std::this_thread::get_id();
  SetLastError(kErrorSuccess);
  return 1;
}
std::int32_t CloseClipboard() {
  std::lock_guard lock(g_mutex);
  if (g_owner != std::this_thread::get_id()) {
    SetLastError(kErrorAccessDenied);
    return 0;
  }
  g_owner = {};
  SetLastError(kErrorSuccess);
  return 1;
}
std::int32_t EmptyClipboard() {
  std::lock_guard lock(g_mutex);
  if (g_owner != std::this_thread::get_id()) {
    SetLastError(kErrorAccessDenied);
    return 0;
  }
  for (const auto& [_, token] : g_formats)
    Handles().Close(token, HandleType::GlobalMemory);
  g_formats.clear();
  SetLastError(kErrorSuccess);
  return 1;
}
std::uint32_t SetClipboardData(std::uint32_t format, std::uint32_t token) {
  std::lock_guard lock(g_mutex);
  if (g_owner != std::this_thread::get_id()) {
    SetLastError(kErrorAccessDenied);
    return 0;
  }
  if (!Handles().IsValid(token, HandleType::GlobalMemory)) return 0;
  if (const auto found = g_formats.find(format); found != g_formats.end())
    Handles().Close(found->second, HandleType::GlobalMemory);
  g_formats[format] = token;
  SetLastError(kErrorSuccess);
  return token;
}
std::uint32_t GetClipboardData(std::uint32_t format) {
  std::lock_guard lock(g_mutex);
  if (g_owner != std::this_thread::get_id()) {
    SetLastError(kErrorAccessDenied);
    return 0;
  }
  const auto found = g_formats.find(format);
  if (found == g_formats.end()) { SetLastError(kErrorFileNotFound); return 0; }
  SetLastError(kErrorSuccess);
  return found->second;
}

template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}  // namespace

ImportAddress ResolveClipboardImport(std::string_view dll, std::string_view symbol) {
  if (dll != "user32.dll") return nullptr;
  if (symbol == "OpenClipboard") return Address(&OpenClipboard);
  if (symbol == "CloseClipboard") return Address(&CloseClipboard);
  if (symbol == "EmptyClipboard") return Address(&EmptyClipboard);
  if (symbol == "SetClipboardData") return Address(&SetClipboardData);
  if (symbol == "GetClipboardData") return Address(&GetClipboardData);
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat
