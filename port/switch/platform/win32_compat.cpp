#include "win32_compat.hpp"

#include "e2e_calendar.hpp"
#include "e2e_clock.hpp"
#include "e2e_stage.hpp"
#include "win32_handles.hpp"
#include "win32_compat_sync.hpp"
#include "win32_compat_files.hpp"
#include "win32_compat_okgf.hpp"
#include "win32_compat_modules.hpp"
#include "win32_compat_process.hpp"
#include "win32_compat_registry.hpp"
#include "win32_compat_filetime.hpp"
#include "win32_compat_window.hpp"
#include "win32_compat_messages.hpp"
#include "win32_compat_gdi.hpp"
#include "win32_compat_text.hpp"
#include "win32_compat_clipboard.hpp"
#include "win32_compat_heap.hpp"
#include "win32_compat_winmm.hpp"
#include "zlib_bridge.hpp"

#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>

namespace srhd_awa::platform::win32_compat {
namespace {
std::atomic<std::uint32_t> g_resolved{0};
std::atomic<std::uint32_t> g_unmapped{0};
std::atomic<std::uint32_t> g_optional{0};

std::string NormalizeModule(std::string_view name) {
  const auto slash = name.find_last_of("/\\");
  if (slash != std::string_view::npos) name.remove_prefix(slash + 1);
  std::string normalized(name);
  for (char& ch : normalized)
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  if (!normalized.ends_with(".dll")) normalized += ".dll";
  return normalized;
}

void GetSystemTimeThunk(Windows::TSystemTime* output) {
  if (output) e2e_calendar::GetSystemTime(*output);
}

void GetLocalTimeThunk(Windows::TSystemTime* output) {
  if (output) e2e_calendar::GetLocalTime(*output);
}

std::int32_t QueryPerformanceCounterThunk(std::int64_t* counter) {
  if (!counter) { SetLastError(kErrorInvalidParameter); return 0; }
  *counter = static_cast<std::int64_t>(e2e_clock::Counter());
  SetLastError(kErrorSuccess);
  return 1;
}

std::int32_t QueryPerformanceFrequencyThunk(std::int64_t* frequency) {
  if (!frequency) { SetLastError(kErrorInvalidParameter); return 0; }
  *frequency = static_cast<std::int64_t>(e2e_clock::Frequency());
  SetLastError(kErrorSuccess);
  return 1;
}

std::uint32_t GetTickCountThunk() { return e2e_clock::Milliseconds(); }
std::uint32_t GetLastErrorThunk() { return GetLastError(); }

template <class Function>
ImportAddress Address(Function function) {
  return reinterpret_cast<ImportAddress>(function);
}

ImportAddress KnownImport(const std::string& dll, std::string_view symbol) {
  if (dll == "okgf.dll") return ResolveOkgfImport(symbol);
  if (dll == "zlib.dll") {
    if (symbol == "OKGF_ZLib_Compress") return Address(&zlib_bridge::Compress);
    if (symbol == "OKGF_ZLib_UnCompress") return Address(&zlib_bridge::Uncompress);
    if (symbol == "OKGF_ZLib_UnCompress2") return Address(&zlib_bridge::UncompressZl02);
  }
  if (const auto module = ResolveModuleImport(dll, symbol)) return module;
  if (const auto process = ResolveProcessImport(dll, symbol)) return process;
  if (const auto registry = ResolveRegistryImport(dll, symbol)) return registry;
  if (const auto filetime = ResolveFileTimeImport(dll, symbol)) return filetime;
  if (const auto window = ResolveWindowImport(dll, symbol)) return window;
  if (const auto messages = ResolveMessageImport(dll, symbol)) return messages;
  if (const auto gdi = ResolveGdiImport(dll, symbol)) return gdi;
  if (const auto value = ResolveTextImport(dll, symbol)) return value;
  if (const auto clipboard = ResolveClipboardImport(dll, symbol)) return clipboard;
  if (const auto heap = ResolveHeapImport(dll, symbol)) return heap;
  if (const auto winmm = ResolveWinmmImport(dll, symbol)) return winmm;
  if (const auto file = ResolveFileImport(dll, symbol)) return file;
  if (const auto sync = ResolveSyncImport(dll, symbol)) return sync;
  if (dll == "kernel32.dll") {
    if (symbol == "GetSystemTime") return Address(&GetSystemTimeThunk);
    if (symbol == "GetLocalTime") return Address(&GetLocalTimeThunk);
    if (symbol == "QueryPerformanceCounter") return Address(&QueryPerformanceCounterThunk);
    if (symbol == "QueryPerformanceFrequency") return Address(&QueryPerformanceFrequencyThunk);
    if (symbol == "GetTickCount") return Address(&GetTickCountThunk);
    if (symbol == "GetLastError") return Address(&GetLastErrorThunk);
  }
  return nullptr;
}
}  // namespace

ImportAddress ResolveImport(const char* library, const char* symbol) {
  if (!library || !symbol) {
    SetLastError(kErrorInvalidParameter);
    throw std::runtime_error("Win32 import has null library or symbol");
  }
  const auto dll = NormalizeModule(library);
  const auto selector = reinterpret_cast<std::uintptr_t>(symbol);
  if (selector <= 0xffffu) {
    ++g_unmapped;
    SetLastError(kErrorFileNotFound);
    const std::string detail = "UNMAPPED stage=" + e2e_stage::CurrentStage() +
        " dll=" + dll + " symbol=#" + std::to_string(selector);
    e2e_stage::LogWinApi(detail.c_str());
    throw std::runtime_error(detail);
  }
  if (const auto address = KnownImport(dll, symbol)) {
    ++g_resolved;
#if defined(E2E_WINAPI_TRACE)
    const std::string detail = "RESOLVED stage=" + e2e_stage::CurrentStage() +
        " dll=" + dll + " symbol=" + symbol;
    e2e_stage::LogWinApi(detail.c_str());
#endif
    return address;
  }
  if (dll == "avifil32.dll" || dll == "dsound.dll" ||
      (dll == "kernel32.dll" &&
       (std::string_view(symbol) == "GetLocaleInfoA" ||
        std::string_view(symbol) == "GetThreadLocale")) ||
      (dll == "ole32.dll" &&
       std::string_view(symbol) != "CreateStreamOnHGlobal") ||
      dll == "shell32.dll") {
    ++g_optional;
    SetLastError(kErrorFileNotFound);
    const std::string detail = "OPTIONAL_DISABLED stage=" + e2e_stage::CurrentStage() +
        " dll=" + dll + " symbol=" + symbol;
    e2e_stage::LogWinApi(detail.c_str());
    throw std::runtime_error(detail);
  }
  ++g_unmapped;
  SetLastError(kErrorInvalidParameter);
  const std::string detail = "UNMAPPED stage=" + e2e_stage::CurrentStage() +
      " dll=" + dll + " symbol=" + symbol;
  e2e_stage::LogWinApi(detail.c_str());
  throw std::runtime_error(detail);
}

std::uint32_t ResolvedImportCount() { return g_resolved.load(); }
std::uint32_t UnmappedImportCount() { return g_unmapped.load(); }
std::uint32_t OptionalDisabledImportCount() { return g_optional.load(); }
std::uint32_t PhysicalDllLoadCount() { return 0; }
void LogRuntimeStats() {
  char detail[240]{};
  std::snprintf(detail, sizeof(detail),
      "resolved=%u optional_disabled=%u unmapped=%u dynamic_loads=%u physical_dll_loads=%u",
      ResolvedImportCount(), OptionalDisabledImportCount(), UnmappedImportCount(),
      DynamicModuleLoadCount(), PhysicalDllLoadCount());
  e2e_stage::LogWinApi(detail);
}
}  // namespace srhd_awa::platform::win32_compat
