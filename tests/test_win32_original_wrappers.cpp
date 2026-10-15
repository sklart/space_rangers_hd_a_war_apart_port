#include "units/WindowsImports.hpp"
#include "units/WindowsSdk.hpp"
#include "units/DirectSound.hpp"
#include "units/VFW.hpp"
#include "units/SysUtilsImports.hpp"
#include "types/Windows_group.hpp"
#include "types/SysUtils.hpp"
#include "win32_compat.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <string>

#if defined(E2E_HOST_OKGF_STUB)
namespace srhd_awa::platform::win32_compat {
ImportAddress ResolveOkgfImport(std::string_view) { return nullptr; }
}
#endif

namespace {
std::atomic<int> g_messages{0};

std::int32_t WindowProc(std::uint32_t, std::uint32_t message,
                        std::int32_t, std::int32_t) {
  if (message == 0x401) ++g_messages;
  return 0;
}

std::int32_t Worker(void* parameter) {
  const auto event = *static_cast<std::uint32_t*>(parameter);
  return WindowsSdk::SetEvent(event) ? 0 : 1;
}
}  // namespace

int main() {
  // Original video-object cleanup is unconditional, including unopened films.
  VFW::AVIFileExit();
  VFW::AVIFileExit();
  bool video_open_disabled = false;
  try { VFW::AVIFileInit(); }
  catch (const std::runtime_error& error) {
    video_open_disabled = std::string(error.what()).find("OPTIONAL_DISABLED") != std::string::npos;
  }
  assert(video_open_disabled);
  Windows::TSystemTime utc{};
  WindowsImports::GetSystemTime(utc);
  assert(utc.wYear >= 2026 && utc.wMonth >= 1 && utc.wMonth <= 12);
  Windows::TSystemTime local{};
  WindowsSdk::GetLocalTime(local);
  assert(local.wYear >= 2026 && local.wMonth >= 1 && local.wMonth <= 12);
  auto event = WindowsImports::CreateEvent(nullptr, 1, 0, nullptr);
  assert(event != 0 && event != 0xffffffffu);
  assert(WindowsSdk::WaitForSingleObject(event, 0) == 258);
  std::uint32_t thread_id = 0;
  const auto thread = WindowsSdk::CreateThread(nullptr, 0,
                                               reinterpret_cast<void*>(&Worker), &event,
                                               4, thread_id);
  assert(thread && thread_id);
  assert(WindowsSdk::ResumeThread(thread) == 1);
  assert(WindowsSdk::WaitForSingleObject(thread, 5000) == 0);
  WindowsSdk::TWOHandleArray event_handles{};
  event_handles[0] = event;
  assert(WindowsSdk::WaitForMultipleObjects(1, &event_handles, 1, 0) == 0);
  assert(WindowsSdk::ResetEvent(event));
  assert(WindowsSdk::WaitForSingleObject(event, 0) == 258);
  assert(WindowsImports::CloseHandle(thread));
  assert(WindowsImports::CloseHandle(event));
  assert(!WindowsImports::CloseHandle(event));
  assert(WindowsImports::GetLastError() == 6);
  assert(WindowsSdk::GetVersion() != 0);
  std::uint8_t module_path[260]{};
  assert(WindowsImports::GetModuleFileNameA(0, module_path, sizeof(module_path)) > 0);
  assert(SysUtilsImports::AnsiLowerCase(pas::AnsiString("RU")) == "ru");
  const auto file_path = std::filesystem::temp_directory_path() /
      "srhd-win32-wrapper-file.bin";
  const auto file_name = file_path.string();
  const auto file = WindowsImports::CreateFileA(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(file_name.c_str())),
      0xc0000000u, 0, nullptr, 2, 0, 0);
  assert(file && file != 0xffffffffu);
  const char payload[] = "wrapper";
  std::uint32_t transferred = 0;
  assert(WindowsImports::WriteFile(file, payload, sizeof(payload) - 1,
                                    transferred, nullptr));
  assert(transferred == sizeof(payload) - 1);
  assert(WindowsImports::SetFilePointer(file, 0, nullptr, 0) == 0);
  char read_back[sizeof(payload)]{};
  assert(WindowsImports::ReadFile(file, read_back, sizeof(payload) - 1,
                                   transferred, nullptr));
  assert(transferred == sizeof(payload) - 1);
  assert(std::strcmp(read_back, payload) == 0);
  const auto denied = WindowsImports::CreateFileA(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(file_name.c_str())),
      0x80000000u, 1, nullptr, 3, 0, 0);
  assert(denied == 0xffffffffu && WindowsImports::GetLastError() == 32);
  assert(WindowsImports::CloseHandle(file));
  const auto reader_a = WindowsImports::CreateFileA(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(file_name.c_str())),
      0x80000000u, 1, nullptr, 3, 0, 0);
  const auto reader_b = WindowsImports::CreateFileA(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(file_name.c_str())),
      0x80000000u, 1, nullptr, 3, 0, 0);
  assert(reader_a != 0xffffffffu && reader_b != 0xffffffffu);
  const auto denied_write = WindowsImports::CreateFileA(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(file_name.c_str())),
      0x40000000u, 3, nullptr, 3, 0, 0);
  assert(denied_write == 0xffffffffu && WindowsImports::GetLastError() == 32);
  assert(WindowsImports::CloseHandle(reader_a));
  assert(WindowsImports::CloseHandle(reader_b));
  Windows::TWin32FindDataA file_data{};
  const auto found = WindowsImports::FindFirstFileA(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(file_name.c_str())),
      file_data);
  assert(found && found != 0xffffffffu);
  assert(WindowsImports::FindClose(found));
  assert(WindowsImports::DeleteFileA(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>(file_name.c_str()))));
  assert(!std::filesystem::exists(file_path));
  const auto search_path = std::filesystem::temp_directory_path() /
      "srhd-win32-wrapper-search.txt";
  { std::ofstream fixture(search_path); fixture << "fixture"; }
  SysUtils::TSearchRec search{};
  const pas::AnsiString pattern(search_path.string().c_str());
  assert(SysUtilsImports::FindFirst(pattern, 0, search) == 0);
  assert(search.FindHandle && search.FindHandle != 0xffffffffu);
  SysUtilsImports::FindClose(search);
  std::filesystem::remove(search_path);
  char16_t class_name[] = u"E2EWrapperClass";
  char16_t window_title[] = u"E2E wrapper window";
  WindowsSdk::TWndClassW window_class{};
  window_class.lpfnWndProc = reinterpret_cast<void*>(&WindowProc);
  window_class.lpszClassName = class_name;
  assert(WindowsSdk::RegisterClassW(window_class));
  const auto window = WindowsImports::CreateWindowExW(0, class_name,
      window_title, 0, 0, 0, 1280, 720, 0, 0, 0, nullptr);
  assert(window);
  assert(WindowsSdk::PostMessage(window, 0x401, 0, 0));
  Windows::TMsg message{};
  assert(WindowsSdk::PeekMessageW(message, window, 0x401, 0x401, 1));
  assert(message.hwnd == window && message.message == 0x401);
  assert(!WindowsSdk::TranslateMessage(message));
  WindowsSdk::DispatchMessageW(message);
  assert(g_messages == 1);
  WindowsSdk::PostQuitMessage(9);
  assert(WindowsSdk::PeekMessageW(message, 0, 0x12, 0x12, 1));
  assert(message.message == 0x12 && message.wParam == 9);
  assert(WindowsSdk::DestroyWindow(window));

  std::uint8_t registry_path[] = "Software\\SpaceRangersE2EWrappers";
  WindowsSdk::HKEY registry_key = 0;
  assert(WindowsSdk::RegOpenKeyExA(0x80000001u, registry_path, 0, 0,
                                   registry_key) == 2);
  assert(registry_key == 0);
  char16_t registry_path_w[] = u"Software\\SpaceRangersE2EWrappers";
  std::uint32_t disposition = 0;
  assert(WindowsSdk::RegCreateKeyExW(0x80000001u, registry_path_w,
      0, nullptr, 0, 0, nullptr, registry_key, &disposition) == 0);
  assert(registry_key && disposition == 1);
  char16_t value_name_w[] = u"Count";
  std::uint32_t value = 17;
  assert(WindowsSdk::RegSetValueExW(registry_key, value_name_w,
      0, 4, &value, sizeof(value)) == 0);
  std::uint8_t value_name_a[] = "Count";
  std::uint32_t value_type = 0, value_size = sizeof(value), observed = 0;
  assert(WindowsSdk::RegQueryValueExA(registry_key, value_name_a,
      nullptr, &value_type, reinterpret_cast<std::uint8_t*>(&observed),
      &value_size) == 0);
  assert(value_type == 4 && value_size == sizeof(value) && observed == value);
  assert(WindowsSdk::RegCloseKey(registry_key) == 0);
  SysUtilsImports::Sleep(1);
  const auto module = WindowsImports::LoadLibrary(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>("PATH\\KERNEL32.DLL")));
  assert(module != 0);
  const auto proc = WindowsImports::GetProcAddress(module,
      reinterpret_cast<std::uint8_t*>(const_cast<char*>("GetSystemTime")));
  assert(proc != nullptr);
  Windows::TSystemTime via_dynamic{};
  reinterpret_cast<void (*)(Windows::TSystemTime*)>(proc)(&via_dynamic);
  assert(via_dynamic.wYear == utc.wYear);
  assert(WindowsImports::GetProcAddress(module,
      reinterpret_cast<std::uint8_t*>(42)) == nullptr);
  assert(WindowsImports::FreeLibrary(module));
  for (const char* disabled : {"steam_ach.dll", "libvorbisfile.dll",
                               "vorbisfile.dll", "xvidcore.dll", "dsound.dll",
                               "avifil32.dll", "d3d9.dll", "ntdll.dll"}) {
    assert(WindowsImports::LoadLibrary(
        reinterpret_cast<std::uint8_t*>(const_cast<char*>(disabled))) == 0);
    assert(WindowsImports::GetLastError() == 2);
  }
  char16_t disabled_wide[] = u"X:\\MUSIC\\LIBVORBISFILE.DLL";
  assert(WindowsSdk::LoadLibraryW(disabled_wide) == 0);
  assert(WindowsImports::GetLastError() == 2);
  assert(WindowsImports::LoadLibrary(
      reinterpret_cast<std::uint8_t*>(const_cast<char*>("MatrixGame.dll"))) == 0);
  assert(WindowsImports::GetLastError() == 2);
  bool audio_disabled = false;
  try { DirectSound::DirectSoundEnumerateA(DirectSound::TDSEnumCallback{}, nullptr); }
  catch (const std::runtime_error& error) {
    audio_disabled = std::string(error.what()).find("OPTIONAL_DISABLED") != std::string::npos;
  }
  assert(audio_disabled && WindowsImports::GetLastError() == 2);
  bool rejected = false;
  try {
    WindowsImports::LoadLibrary(
        reinterpret_cast<std::uint8_t*>(const_cast<char*>("unknown-required.dll")));
  } catch (const std::runtime_error& error) {
    rejected = std::string(error.what()).find("unknown-required.dll") != std::string::npos;
  }
  assert(rejected);
}
