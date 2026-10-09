#include "win32_compat_process.hpp"

#include "e2e_clock.hpp"
#include "e2e_stage.hpp"
#include "win32_handles.hpp"
#include "types/Windows_group.hpp"
#include "types/GR_Main.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace srhd_awa::platform::win32_compat {
namespace {
constexpr std::uint32_t kCurrentProcess = 0xffffffffu;
constexpr std::uint32_t kNormalPriority = 0x20u;
std::atomic<std::uint32_t> g_priority{kNormalPriority};
struct GlobalBlock {
  std::mutex mutex;
  std::vector<std::uint8_t> bytes;
  std::uint32_t locks = 0;
};

std::uint8_t* CommandLine() {
  static std::uint8_t line[] = "Rangers";
  return line;
}

std::uint32_t ModuleFileName(std::uint32_t module, std::uint8_t* buffer,
                             std::uint32_t capacity) {
  if (module != 0 || !buffer || !capacity) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  const std::string name = std::string(e2e_stage::kRoot) + "/game/Rangers.exe";
  const auto count = std::min<std::size_t>(name.size(), capacity - 1);
  std::memcpy(buffer, name.data(), count);
  buffer[count] = 0;
  SetLastError(count < name.size() ? kErrorInvalidParameter : kErrorSuccess);
  return static_cast<std::uint32_t>(count);
}

std::uint32_t CurrentProcess() { return kCurrentProcess; }

std::uint32_t GetVersion() {
  // Win32-compatible version metadata for legacy feature checks.
  return (1u << 8) | 6u;
}

std::int32_t GetVersionEx(WindowsSdk::TOSVersionInfo* output) {
  if (!output || output->dwOSVersionInfoSize < sizeof(*output)) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  output->dwMajorVersion = 6;
  output->dwMinorVersion = 1;
  output->dwBuildNumber = 7601;
  output->dwPlatformId = 2;
  std::memset(output->szCSDVersion.elements, 0,
              sizeof(output->szCSDVersion.elements));
  SetLastError(kErrorSuccess);
  return 1;
}

std::uint32_t GetSystemDirectoryW(char16_t*, std::uint32_t) {
  SetLastError(kErrorPathNotFound);
  return 0; // The generated GR_Main source omits the DirectX system DLL path.
}

std::uint32_t GlobalAlloc(std::uint32_t flags, std::uint32_t bytes) {
  if (flags & ~(0x40u | 0x2u)) { SetLastError(kErrorInvalidParameter); return 0; }
  auto block = std::make_shared<GlobalBlock>();
  block->bytes.resize(std::max<std::uint32_t>(bytes, 1));
  return Handles().Allocate(HandleType::GlobalMemory, std::move(block));
}

void* GlobalLock(std::uint32_t token) {
  const auto block = std::static_pointer_cast<GlobalBlock>(
      Handles().Lookup(token, HandleType::GlobalMemory));
  if (!block) return nullptr;
  std::lock_guard lock(block->mutex);
  ++block->locks;
  return block->bytes.data();
}

std::int32_t GlobalUnlock(std::uint32_t token) {
  const auto block = std::static_pointer_cast<GlobalBlock>(
      Handles().Lookup(token, HandleType::GlobalMemory));
  if (!block) return 0;
  std::lock_guard lock(block->mutex);
  if (!block->locks) { SetLastError(kErrorInvalidParameter); return 0; }
  --block->locks;
  SetLastError(kErrorSuccess);
  return block->locks ? 1 : 0; // Win32 returns zero when the final lock is released.
}

bool Memory(std::uint64_t* total, std::uint64_t* free) {
#if defined(__SWITCH__)
  std::uint64_t used = 0;
  if (R_FAILED(svcGetInfo(total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0)) ||
      R_FAILED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0)) ||
      !*total || used > *total) return false;
  *free = *total - used;
  return true;
#else
  // Host resolver tests use only nonzero, internally consistent values.
  *total = 1024ull * 1024ull * 1024ull;
  *free = *total / 2;
  return true;
#endif
}

void GlobalMemoryStatus(WindowsSdk::TMemoryStatus* out) {
  if (!out) { SetLastError(kErrorInvalidParameter); return; }
  std::uint64_t total = 0, free = 0;
  if (!Memory(&total, &free)) { SetLastError(kErrorAccessDenied); return; }
  out->dwLength = sizeof(*out);
  out->dwMemoryLoad = static_cast<std::uint32_t>((total - free) * 100 / total);
  out->dwTotalPhys = static_cast<std::uint32_t>(std::min<std::uint64_t>(total, 0xffffffffu));
  out->dwAvailPhys = static_cast<std::uint32_t>(std::min<std::uint64_t>(free, 0xffffffffu));
  out->dwTotalPageFile = out->dwTotalPhys;
  out->dwAvailPageFile = out->dwAvailPhys;
  out->dwTotalVirtual = out->dwTotalPhys;
  out->dwAvailVirtual = out->dwAvailPhys;
  SetLastError(kErrorSuccess);
}

std::int32_t GlobalMemoryStatusEx(GR_Main::TMemoryStatusEx* out) {
  if (!out || out->Length < sizeof(*out)) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  std::uint64_t total = 0, free = 0;
  if (!Memory(&total, &free)) { SetLastError(kErrorAccessDenied); return 0; }
  out->MemoryLoad = static_cast<std::uint32_t>((total - free) * 100 / total);
  out->TotalPhys = total; out->AvailPhys = free;
  out->TotalPageFile = total; out->AvailPageFile = free;
  out->TotalVirtual = total; out->AvailVirtual = free;
  out->AvailExtendedVirtual = 0;
  SetLastError(kErrorSuccess);
  return 1;
}

void GetSystemInfo(WindowsSdk::TSystemInfo* out) {
  if (!out) { SetLastError(kErrorInvalidParameter); return; }
  *out = {};
  out->dwOemId = 0; // ARM64, not an x86/WOW64 host.
  SetLastError(kErrorSuccess);
}

std::int32_t SetPriorityClass(std::uint32_t process, std::uint32_t priority) {
  if (process != kCurrentProcess || !priority) {
    SetLastError(kErrorInvalidParameter);
    return 0;
  }
  g_priority = priority; // Logical priority only; Switch scheduling is owned by OS.
  SetLastError(kErrorSuccess);
  return 1;
}

std::uint32_t GetPriorityClass(std::uint32_t process) {
  if (process != kCurrentProcess) { SetLastError(kErrorInvalidHandle); return 0; }
  SetLastError(kErrorSuccess);
  return g_priority;
}

std::int32_t SetProcessAffinityMask(std::uint32_t process, std::uint32_t) {
  SetLastError(process == kCurrentProcess ? kErrorAccessDenied : kErrorInvalidHandle);
  return 0; // E2E source adaptation omits native CPU-affinity probing.
}

void Sleep(std::uint32_t milliseconds) { e2e_clock::SleepMilliseconds(milliseconds); }

template <class F> ImportAddress Address(F function) {
  return reinterpret_cast<ImportAddress>(function);
}
}  // namespace

ImportAddress ResolveProcessImport(std::string_view dll, std::string_view symbol) {
  if (dll != "kernel32.dll") return nullptr;
  if (symbol == "GetCommandLineA") return Address(&CommandLine);
  if (symbol == "GetModuleFileNameA") return Address(&ModuleFileName);
  if (symbol == "GetCurrentProcess") return Address(&CurrentProcess);
  if (symbol == "GetVersion") return Address(&GetVersion);
  if (symbol == "GetVersionExA") return Address(&GetVersionEx);
  if (symbol == "GetSystemDirectoryW") return Address(&GetSystemDirectoryW);
  if (symbol == "GlobalAlloc") return Address(&GlobalAlloc);
  if (symbol == "GlobalLock") return Address(&GlobalLock);
  if (symbol == "GlobalUnlock") return Address(&GlobalUnlock);
  if (symbol == "GlobalMemoryStatus") return Address(&GlobalMemoryStatus);
  if (symbol == "GlobalMemoryStatusEx") return Address(&GlobalMemoryStatusEx);
  if (symbol == "GetSystemInfo") return Address(&GetSystemInfo);
  if (symbol == "SetPriorityClass") return Address(&SetPriorityClass);
  if (symbol == "GetPriorityClass") return Address(&GetPriorityClass);
  if (symbol == "SetProcessAffinityMask") return Address(&SetProcessAffinityMask);
  if (symbol == "Sleep") return Address(&Sleep);
  return nullptr;
}
}  // namespace srhd_awa::platform::win32_compat
