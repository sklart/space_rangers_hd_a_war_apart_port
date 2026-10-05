#include "ec_file_adapter.hpp"
#include "package.hpp"
#include "units/EC_HsFile.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {
std::vector<std::unique_ptr<srhd_awa::package::Package>> g_packages;
EC_HsFile::TPackCollectionEC g_collection;
pas::CriticalSection g_lock;

bool TraceEnabled() {
#if defined(__SWITCH__)
  return false;
#else
  static const bool enabled = std::getenv("SRHD_EC_FILE_TRACE") != nullptr;
  return enabled;
#endif
}

void Trace(const char* operation, std::int32_t handle, std::uint32_t value = 0) {
  if (TraceEnabled()) std::fprintf(stderr, "[EC_FILE] %s handle=%d value=%u\n", operation, handle, value);
}
}

namespace EC_HsFile {
TPackCollectionEC* PackageCollection = nullptr;
pas::CriticalSection* PackageFileLock = nullptr;
pas::AnsiString LooseFileRoot{};
const pas::WideString PackSlotRangeError{};

void TPackCollectionEC::p_destroy() {}
std::int32_t TPackCollectionEC::OpenEntryByPathAcrossPackages(
    pas::AnsiString path, std::uint32_t, std::uint8_t first_package_only) {
  const size_t limit = first_package_only ? std::min<size_t>(1, g_packages.size()) : g_packages.size();
  for (size_t i = 0; i < limit; ++i) {
    std::string error;
    const auto slot = g_packages[i]->OpenEntryByPath(path.c_str(), &error);
    if (slot) {
      const auto handle = static_cast<std::int32_t>(i * 16 + slot - 1);
      if (TraceEnabled()) std::fprintf(stderr, "[EC_FILE] open path=%s handle=%d\n", path.c_str(), handle);
      return handle;
    }
  }
  return -1;
}
std::int32_t TPackCollectionEC::CreateLooseFile(pas::WideString) { return -1; }
std::uint8_t TPackCollectionEC::CloseEntryHandle(std::int32_t handle) {
  if (handle < 0) return false;
  const size_t index = static_cast<size_t>(handle / 16);
  const uint32_t slot = static_cast<uint32_t>(handle % 16 + 1);
  if (index >= g_packages.size() || g_packages[index]->GetEntryPosition(slot) == UINT64_MAX) return false;
  g_packages[index]->CloseEntry(slot);
  Trace("close", handle);
  return true;
}
std::uint8_t TPackCollectionEC::ReadEntryHandle(std::int32_t handle, void* buffer, std::uint32_t bytes) {
  if (handle < 0) return false;
  const size_t index = static_cast<size_t>(handle / 16);
  if (index >= g_packages.size()) return false;
  std::string error;
  const bool ok = g_packages[index]->ReadEntry(static_cast<uint32_t>(handle % 16 + 1), buffer, bytes, &error) == bytes;
  if (ok) Trace("read", handle, bytes);
  return ok;
}
std::uint8_t TPackCollectionEC::WriteEntryHandle(std::int32_t, void*, std::uint32_t) { return false; }
std::uint8_t TPackCollectionEC::SeekEntryHandle(std::int32_t handle, std::uint32_t offset, std::int32_t origin) {
  if (handle < 0) return false;
  std::string error;
  const size_t index=static_cast<size_t>(handle/16);
  const bool ok = index < g_packages.size() && g_packages[index]->SeekEntry(static_cast<uint32_t>(handle%16+1),offset,static_cast<uint32_t>(origin),&error);
  if (ok) Trace("seek", handle, offset);
  return ok;
}
std::uint32_t TPackCollectionEC::GetEntryHandlePosition(std::int32_t handle) {
  if (handle < 0) return 0xffffffffu;
  const size_t index = static_cast<size_t>(handle / 16);
  if (index >= g_packages.size()) return 0xffffffffu;
  const auto value = g_packages[index]->GetEntryPosition(static_cast<uint32_t>(handle%16+1));
  return value == UINT64_MAX || value > UINT32_MAX ? 0xffffffffu : static_cast<uint32_t>(value);
}
std::uint32_t TPackCollectionEC::GetEntryHandleSize(std::int32_t handle) {
  if (handle < 0) return 0xffffffffu;
  const size_t index = static_cast<size_t>(handle / 16);
  if (index >= g_packages.size()) return 0xffffffffu;
  const auto value = g_packages[index]->GetEntrySize(static_cast<uint32_t>(handle%16+1));
  return value == UINT64_MAX || value > UINT32_MAX ? 0xffffffffu : static_cast<uint32_t>(value);
}
}

namespace srhd_awa::platform::ec_file {
bool OpenPackage(const std::string& path, std::string* error) {
  return OpenPackages({path}, error);
}
bool OpenPackages(const std::vector<std::string>& paths, std::string* error) {
  ClosePackage();
  for (const auto& path : paths) {
    auto package = std::make_unique<srhd_awa::package::Package>();
    if (!package->Open(path, error)) {
      ClosePackage();
      return false;
    }
    g_packages.push_back(std::move(package));
  }
  if (g_packages.empty()) {
    *error = "empty package collection";
    return false;
  }
  g_lock.p_create();
  EC_HsFile::PackageCollection = &g_collection;
  EC_HsFile::PackageFileLock = &g_lock;
  return true;
}
void ClosePackage() {
  g_packages.clear();
  EC_HsFile::PackageCollection = nullptr;
  EC_HsFile::PackageFileLock = nullptr;
}
}
