#pragma once

#include <cstddef>
#include <cstdint>

namespace srhd_awa::platform {

// On-disk package values are Win32 game-format values, never native pointers.
struct PackageRootInfo {
  std::uint32_t root_offset{};
  std::uint32_t header_size{};
  std::uint32_t entry_count{};
  std::uint32_t entry_record_size{};
  std::uint64_t file_size{};
  char first_entry_name[64]{};
};

// First stage of EC_HsFile::TPackFileEC::Open and THsFolderEC::Load.
bool ProbePackageRoot(const char* game_root, const char* relative_path,
                      PackageRootInfo* info, char* error, std::size_t error_size);

}  // namespace srhd_awa::platform
