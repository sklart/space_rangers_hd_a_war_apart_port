#include "units/WindowsImports.hpp"
#include "units/WindowsSdk.hpp"
#include "types/Windows_group.hpp"
#include "win32_compat.hpp"

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>

#if defined(E2E_HOST_OKGF_STUB)
namespace srhd_awa::platform::win32_compat {
ImportAddress ResolveOkgfImport(std::string_view) { return nullptr; }
}
#endif

int main() {
  Windows::TSystemTime utc{};
  WindowsImports::GetSystemTime(utc);
  assert(utc.wYear >= 2026 && utc.wMonth >= 1 && utc.wMonth <= 12);
  const auto event = WindowsImports::CreateEvent(nullptr, 1, 0, nullptr);
  assert(event != 0 && event != 0xffffffffu);
  assert(WindowsImports::CloseHandle(event));
  assert(!WindowsImports::CloseHandle(event));
  assert(WindowsImports::GetLastError() == 6);
  assert(WindowsSdk::GetVersion() != 0);
  const auto module = WindowsImports::LoadLibrary(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>("kernel32.dll")));
  assert(module != 0);
  const auto proc = WindowsImports::GetProcAddress(module,
      reinterpret_cast<std::uint8_t*>(const_cast<char*>("GetSystemTime")));
  assert(proc != nullptr);
  Windows::TSystemTime via_dynamic{};
  reinterpret_cast<void (*)(Windows::TSystemTime*)>(proc)(&via_dynamic);
  assert(via_dynamic.wYear == utc.wYear);
  assert(WindowsImports::FreeLibrary(module));
  assert(WindowsImports::LoadLibrary(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>("steam_ach.dll"))) == 0);
  bool rejected = false;
  try {
    WindowsImports::LoadLibrary(
        reinterpret_cast<std::uint8_t*>(const_cast<char*>("unknown-required.dll")));
  } catch (const std::runtime_error& error) {
    rejected = std::string(error.what()).find("unknown-required.dll") != std::string::npos;
  }
  assert(rejected);
}
