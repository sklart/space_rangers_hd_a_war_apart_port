#include "units/WindowsImports.hpp"
#include "units/WindowsSdk.hpp"
#include "units/SysUtilsImports.hpp"
#include "types/Windows_group.hpp"
#include "types/SysUtils.hpp"
#include "win32_compat.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
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
  std::uint8_t module_path[260]{};
  assert(WindowsImports::GetModuleFileNameA(0, module_path, sizeof(module_path)) > 0);
  assert(SysUtilsImports::AnsiLowerCase(pas::AnsiString("RU")) == "ru");
  const auto search_path = std::filesystem::temp_directory_path() /
      "srhd-win32-wrapper-search.txt";
  { std::ofstream fixture(search_path); fixture << "fixture"; }
  SysUtils::TSearchRec search{};
  const pas::AnsiString pattern(search_path.string().c_str());
  assert(SysUtilsImports::FindFirst(pattern, 0, search) == 0);
  assert(search.FindHandle && search.FindHandle != 0xffffffffu);
  SysUtilsImports::FindClose(search);
  std::filesystem::remove(search_path);
  SysUtilsImports::Sleep(1);
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
