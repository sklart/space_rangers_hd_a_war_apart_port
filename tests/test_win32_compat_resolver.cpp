#include "win32_compat.hpp"
#include "win32_handles.hpp"
#include "types/Windows_group.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

#if defined(E2E_HOST_OKGF_STUB)
namespace srhd_awa::platform::win32_compat {
// The ARM64 full link checks all 88 real OKGF export addresses. Host tests
// exercise the remaining platform resolver without a separate host OKGF build.
ImportAddress ResolveOkgfImport(std::string_view) { return nullptr; }
}
#endif

using namespace srhd_awa::platform::win32_compat;

int main() {
  struct ImportCase { const char* dll; const char* symbol; };
  constexpr ImportCase portable_cases[] = {
#include "win32_portable_cases.inc"
  };
  for (const auto& item : portable_cases)
    assert(ResolveImport(item.dll, item.symbol) != nullptr);
  using Calendar = void (*)(Windows::TSystemTime*);
  for (const char* name : {"KERNEL32.DLL", "kernel32.dll",
                           "Kernel32.dll", "path/to/kernel32.dll"}) {
    auto calendar = reinterpret_cast<Calendar>(ResolveImport(name, "GetSystemTime"));
    assert(calendar);
    Windows::TSystemTime utc{};
    calendar(&utc);
    assert(utc.wYear >= 2026 && utc.wMonth >= 1 && utc.wMonth <= 12);
    assert(utc.wMilliseconds < 1000);
  }
  using Counter = std::int32_t (*)(std::int64_t*);
  auto count = reinterpret_cast<Counter>(ResolveImport("kernel32", "QueryPerformanceCounter"));
  auto frequency = reinterpret_cast<Counter>(ResolveImport("kernel32.dll", "QueryPerformanceFrequency"));
  std::int64_t ticks = 0;
  std::int64_t hz = 0;
  assert(count(&ticks) && frequency(&hz) && ticks > 0 && hz > 0);
  assert(!count(nullptr) && GetLastError() == kErrorInvalidParameter);
  using CreateEvent = std::uint32_t (*)(void*, std::int32_t, std::int32_t, std::uint8_t*);
  using EventOp = std::int32_t (*)(std::uint32_t);
  using WaitOne = std::uint32_t (*)(std::uint32_t, std::uint32_t);
  using WaitMany = std::uint32_t (*)(std::uint32_t, const std::uint32_t*,
                                     std::int32_t, std::uint32_t);
  const auto create_event = reinterpret_cast<CreateEvent>(ResolveImport("kernel32.dll", "CreateEventA"));
  const auto set_event = reinterpret_cast<EventOp>(ResolveImport("kernel32.dll", "SetEvent"));
  const auto reset_event = reinterpret_cast<EventOp>(ResolveImport("kernel32.dll", "ResetEvent"));
  const auto close = reinterpret_cast<EventOp>(ResolveImport("kernel32.dll", "CloseHandle"));
  const auto wait_one = reinterpret_cast<WaitOne>(ResolveImport("kernel32.dll", "WaitForSingleObject"));
  const auto wait_many = reinterpret_cast<WaitMany>(ResolveImport("kernel32.dll", "WaitForMultipleObjects"));
  const auto event = create_event(nullptr, 1, 0, nullptr);
  assert(event && wait_one(event, 0) == 258);
  assert(set_event(event) && wait_one(event, 0) == 0);
  const std::uint32_t events[] = {event};
  assert(wait_many(1, events, 0, 0) == 0);
  assert(reset_event(event) && wait_one(event, 0) == 258);
  assert(close(event) && !close(event));
  assert(GetLastError() == kErrorInvalidHandle);
  const auto path = (std::filesystem::temp_directory_path() /
                     "srhd-win32-compat-resolver-test.bin").string();
  using CreateFile = std::uint32_t (*)(std::uint8_t*, std::uint32_t,
      std::uint32_t, void*, std::uint32_t, std::uint32_t, std::uint32_t);
  using Transfer = std::int32_t (*)(std::uint32_t, void*, std::uint32_t,
                                    std::uint32_t*, void*);
  using Seek = std::uint32_t (*)(std::uint32_t, std::int32_t,
                                 std::int32_t*, std::uint32_t);
  const auto create_file = reinterpret_cast<CreateFile>(ResolveImport("kernel32", "CreateFileA"));
  const auto write = reinterpret_cast<Transfer>(ResolveImport("kernel32", "WriteFile"));
  const auto read = reinterpret_cast<Transfer>(ResolveImport("kernel32", "ReadFile"));
  const auto seek = reinterpret_cast<Seek>(ResolveImport("kernel32", "SetFilePointer"));
  const auto file = create_file(reinterpret_cast<std::uint8_t*>(const_cast<char*>(path.c_str())),
                                0xc0000000u, 0, nullptr, 2, 0, 0);
  assert(file != 0xffffffffu && file != 0);
  char content[] = "test";
  std::uint32_t done = 0;
  assert(write(file, content, 4, &done, nullptr) && done == 4);
  assert(seek(file, 0, nullptr, 0) == 0);
  char output[4]{};
  assert(read(file, output, 4, &done, nullptr) && done == 4);
  assert(std::string(output, 4) == "test");
  assert(!wait_one(file, 0) || GetLastError() == kErrorInvalidHandle);
  assert(close(file) && !close(file));
  std::filesystem::remove(path);
  using CreateFileW = std::uint32_t (*)(char16_t*, std::uint32_t,
      std::uint32_t, void*, std::uint32_t, std::uint32_t, std::uint32_t);
  auto create_wide = reinterpret_cast<CreateFileW>(
      ResolveImport("kernel32.dll", "CreateFileW"));
  auto wide_path = std::filesystem::temp_directory_path().u16string() +
      u"/srhd-win32-тест.bin";
  const auto wide_file = create_wide(wide_path.data(), 0xc0000000u,
                                     0, nullptr, 2, 0, 0);
  assert(wide_file && wide_file != 0xffffffffu && close(wide_file));
  using FindFirstW = std::uint32_t (*)(char16_t*, WindowsSdk::TWin32FindDataW*);
  auto find_wide = reinterpret_cast<FindFirstW>(
      ResolveImport("kernel32.dll", "FindFirstFileW"));
  WindowsSdk::TWin32FindDataW wide_data{};
  const auto find = find_wide(wide_path.data(), &wide_data);
  assert(find && find != 0xffffffffu);
  assert(std::u16string(wide_data.cFileName.elements) == u"srhd-win32-тест.bin");
  using FindClose = std::int32_t (*)(std::uint32_t);
  assert(reinterpret_cast<FindClose>(ResolveImport("kernel32.dll", "FindClose"))(find));
  std::filesystem::remove(std::filesystem::path(wide_path));
  using RegCreate = std::uint32_t (*)(std::uint32_t, char16_t*, std::uint32_t,
      char16_t*, std::uint32_t, std::uint32_t, void*, std::uint32_t*, std::uint32_t*);
  using RegSet = std::uint32_t (*)(std::uint32_t, char16_t*, std::uint32_t,
      std::uint32_t, void*, std::uint32_t);
  using RegQuery = std::uint32_t (*)(std::uint32_t, std::uint8_t*, void*,
      std::uint32_t*, std::uint8_t*, std::uint32_t*);
  using RegClose = std::uint32_t (*)(std::uint32_t);
  const auto reg_create = reinterpret_cast<RegCreate>(
      ResolveImport("advapi32.dll", "RegCreateKeyExW"));
  const auto reg_set = reinterpret_cast<RegSet>(
      ResolveImport("advapi32.dll", "RegSetValueExW"));
  const auto reg_query = reinterpret_cast<RegQuery>(
      ResolveImport("advapi32.dll", "RegQueryValueExA"));
  const auto reg_close = reinterpret_cast<RegClose>(
      ResolveImport("advapi32.dll", "RegCloseKey"));
  char16_t key_name[] = u"Software\\SpaceRangersE2ETest";
  std::uint32_t key = 0, disposition = 0;
  assert(reg_create(0x80000001u, key_name, 0, nullptr, 0, 0, nullptr,
                    &key, &disposition) == 0 && key);
  char16_t value_name[] = u"Enabled";
  std::uint32_t value = 7;
  assert(reg_set(key, value_name, 0, 4, &value, sizeof(value)) == 0);
  std::uint8_t query_name[] = "Enabled";
  std::uint32_t value_type = 0, capacity = sizeof(value), actual = 0;
  assert(reg_query(key, query_name, nullptr, &value_type,
                   reinterpret_cast<std::uint8_t*>(&actual), &capacity) == 0);
  assert(value_type == 4 && capacity == sizeof(value) && actual == 7);
  assert(reg_close(key) == 0 && reg_close(key) == kErrorInvalidHandle);
  bool failed = false;
  try { ResolveImport("kernel32.dll", "UnknownCriticalApi"); }
  catch (const std::runtime_error& error) {
    failed = std::string(error.what()).find("symbol=UnknownCriticalApi") != std::string::npos;
  }
  assert(failed && UnmappedImportCount() == 1 && ResolvedImportCount() >= 12);
}
