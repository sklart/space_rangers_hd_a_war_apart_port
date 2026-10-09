#include "win32_compat_modules.hpp"

#include "e2e_stage.hpp"
#include "win32_compat_okgf.hpp"
#include "win32_handles.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace srhd_awa::platform::win32_compat {
namespace {
struct Module { std::string name; std::uint32_t refs = 1; };
std::mutex g_mutex;
std::unordered_map<std::string, std::uint32_t> g_loaded;
std::atomic<std::uint32_t> g_loads{0};

std::string Normalize(std::string_view name) {
  const auto slash = name.find_last_of("/\\");
  if (slash != std::string_view::npos) name.remove_prefix(slash + 1);
  std::string normalized(name);
  for (char& c : normalized)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (!normalized.ends_with(".dll")) normalized += ".dll";
  return normalized;
}

std::string Narrow(const char16_t* text) {
  if (!text) return {};
  std::string result;
  for (; *text; ++text) {
    if (*text > 127) return {};
    result.push_back(static_cast<char>(*text));
  }
  return result;
}

bool Optional(std::string_view dll) {
  return dll == "steam_ach.dll" || dll == "vorbisfile.dll" ||
         dll == "libvorbisfile.dll" || dll == "xvidcore.dll" ||
         dll == "dsound.dll" || dll == "avifil32.dll" ||
         dll == "d3d9.dll" || dll == "ntdll.dll";
}

bool Builtin(std::string_view dll) {
  return dll == "okgf.dll" || dll == "kernel32.dll" ||
         dll == "user32.dll" || dll == "gdi32.dll" ||
         dll == "advapi32.dll" || dll == "zlib.dll";
}

std::uint32_t Load(const char* name, bool acquire) {
  if (!name || !*name) { SetLastError(kErrorInvalidParameter); return 0; }
  if (acquire) ++g_loads;
  const auto dll = Normalize(name);
  if (Optional(dll)) { SetLastError(kErrorFileNotFound); return 0; }
  if (dll == "matrixgame.dll") {
    SetLastError(kErrorFileNotFound);
    return 0; // Robot::InitializeRobotRuntime accepts a missing module.
  }
  if (!Builtin(dll)) {
    SetLastError(kErrorFileNotFound);
    const auto detail = "UNMAPPED MODULE stage=" + e2e_stage::CurrentStage() +
                        " dll=" + dll;
    e2e_stage::LogWinApi(detail.c_str());
    throw std::runtime_error(detail);
  }
  std::lock_guard lock(g_mutex);
  if (auto found = g_loaded.find(dll); found != g_loaded.end()) {
    const auto module = std::static_pointer_cast<Module>(
        Handles().Lookup(found->second, HandleType::Module));
    if (module) {
      if (acquire) ++module->refs;
      SetLastError(kErrorSuccess);
      return found->second;
    }
    g_loaded.erase(found);
  }
  const auto token = Handles().Allocate(HandleType::Module,
                                       std::make_shared<Module>(Module{dll}));
  if (token) g_loaded.emplace(dll, token);
  return token;
}

std::uint32_t LoadA(std::uint8_t* name) {
  return Load(reinterpret_cast<const char*>(name), true);
}
std::uint32_t LoadW(char16_t* name) {
  const auto narrow = Narrow(name);
  if (narrow.empty()) { SetLastError(kErrorInvalidParameter); return 0; }
  return Load(narrow.c_str(), true);
}
std::uint32_t FindModule(const char* name) {
  if (!name || !*name) { SetLastError(kErrorInvalidParameter); return 0; }
  const auto dll = Normalize(name);
  if (!Builtin(dll)) { SetLastError(kErrorFileNotFound); return 0; }
  return Load(dll.c_str(), false);
}
std::uint32_t GetModuleA(std::uint8_t* name) {
  return FindModule(reinterpret_cast<const char*>(name));
}
std::uint32_t GetModuleW(char16_t* name) {
  const auto narrow = Narrow(name);
  if (narrow.empty()) { SetLastError(kErrorInvalidParameter); return 0; }
  return FindModule(narrow.c_str());
}

void* GetProc(std::uint32_t token, std::uint8_t* name) {
  const auto module = std::static_pointer_cast<Module>(
      Handles().Lookup(token, HandleType::Module));
  if (!module || !name) { SetLastError(kErrorInvalidHandle); return nullptr; }
  if (reinterpret_cast<std::uintptr_t>(name) <= 0xffffu) {
    SetLastError(kErrorFileNotFound);
    const auto detail = "UNMAPPED EXPORT ordinal stage=" + e2e_stage::CurrentStage() +
                        " dll=" + module->name + " number=" +
                        std::to_string(reinterpret_cast<std::uintptr_t>(name));
    e2e_stage::LogWinApi(detail.c_str());
    return nullptr;
  }
  const auto symbol = std::string_view(reinterpret_cast<const char*>(name));
  ImportAddress result = nullptr;
  if (module->name == "okgf.dll") result = ResolveOkgfImport(symbol);
  else if ((module->name == "kernel32.dll" &&
            (symbol == "GetNativeSystemInfo" || symbol == "IsWow64Process" ||
             symbol == "Wow64DisableWow64FsRedirection" ||
             symbol == "GetSystemWow64DirectoryA")) ||
           (module->name == "advapi32.dll" && symbol == "RegDeleteKeyExA")) {
    // The legacy host detection probes these exports and handles absence.
    SetLastError(kErrorFileNotFound);
    return nullptr;
  } else result = ResolveImport(module->name.c_str(),
                                reinterpret_cast<const char*>(name));
  if (!result) {
    SetLastError(kErrorFileNotFound);
    const auto detail = "UNMAPPED EXPORT stage=" + e2e_stage::CurrentStage() +
                        " dll=" + module->name + " symbol=" + std::string(symbol);
    e2e_stage::LogWinApi(detail.c_str());
    return nullptr;
  }
  SetLastError(kErrorSuccess);
  return reinterpret_cast<void*>(result);
}

std::int32_t Free(std::uint32_t token) {
  std::lock_guard lock(g_mutex);
  const auto module = std::static_pointer_cast<Module>(
      Handles().Lookup(token, HandleType::Module));
  if (!module) return 0;
  if (module->refs > 1) { --module->refs; SetLastError(kErrorSuccess); return 1; }
  g_loaded.erase(module->name);
  return Handles().Close(token, HandleType::Module) ? 1 : 0;
}

template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}  // namespace

ImportAddress ResolveModuleImport(std::string_view dll, std::string_view symbol) {
  if (dll != "kernel32.dll") return nullptr;
  if (symbol == "LoadLibraryA") return Address(&LoadA);
  if (symbol == "LoadLibraryW") return Address(&LoadW);
  if (symbol == "GetModuleHandleA") return Address(&GetModuleA);
  if (symbol == "GetModuleHandleW") return Address(&GetModuleW);
  if (symbol == "GetProcAddress") return Address(&GetProc);
  if (symbol == "FreeLibrary") return Address(&Free);
  return nullptr;
}
std::uint32_t DynamicModuleLoadCount() { return g_loads.load(); }
}  // namespace srhd_awa::platform::win32_compat
