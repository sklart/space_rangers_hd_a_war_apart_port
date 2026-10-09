#pragma once

#include <cstdint>
#include <memory>

namespace srhd_awa::platform::win32_compat {
enum class HandleType : std::uint8_t {
  File, Event, Thread, Window, Module, GdiObject, Timer, Find, GlobalMemory, RegistryKey
};

enum : std::uint32_t {
  kErrorSuccess = 0,
  kErrorFileNotFound = 2,
  kErrorPathNotFound = 3,
  kErrorAccessDenied = 5,
  kErrorInvalidHandle = 6,
  kErrorInvalidParameter = 87,
  kErrorAlreadyExists = 183,
  kErrorNoMoreFiles = 18,
};

void SetLastError(std::uint32_t error);
std::uint32_t GetLastError();

class WinHandleTable {
 public:
  std::uint32_t Allocate(HandleType type, std::shared_ptr<void> object);
  std::shared_ptr<void> Lookup(std::uint32_t handle, HandleType type);
  // Use only while probing several valid handle types before reporting failure.
  std::shared_ptr<void> TryLookup(std::uint32_t handle, HandleType type);
  bool Close(std::uint32_t handle, HandleType type);
  bool IsValid(std::uint32_t handle, HandleType type);
};

WinHandleTable& Handles();
}  // namespace srhd_awa::platform::win32_compat
