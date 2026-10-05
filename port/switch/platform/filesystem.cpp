#include "filesystem.hpp"

#include <cstdio>
#include <cstring>

namespace srhd_awa::platform {
namespace {
constexpr std::size_t kFolderHeaderSize = 12;
constexpr std::uint32_t kReleasePackageEntryRecordSize = 158;

std::uint32_t ReadLe32(const std::uint8_t* bytes) {
  return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
      (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}
void SetError(char* output, std::size_t size, const char* text) {
  if (size != 0) std::snprintf(output, size, "%s", text);
}
}  // namespace

bool ProbePackageRoot(const char* game_root, const char* relative_path,
                      PackageRootInfo* info, char* error, std::size_t error_size) {
  if (!game_root || !relative_path || !info) {
    SetError(error, error_size, "invalid arguments");
    return false;
  }
  *info = {};
  char path[512];
  std::snprintf(path, sizeof(path), "%s/%s", game_root, relative_path);
  std::FILE* file = std::fopen(path, "rb");
  if (!file) {
    SetError(error, error_size, "open failed");
    return false;
  }
  if (std::fseek(file, 0, SEEK_END) != 0) {
    std::fclose(file); SetError(error, error_size, "seek end failed"); return false;
  }
  const long file_size = std::ftell(file);
  if (file_size < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
    std::fclose(file); SetError(error, error_size, "size query failed"); return false;
  }
  std::uint8_t root_offset_bytes[4];
  if (std::fread(root_offset_bytes, 1, sizeof(root_offset_bytes), file) != sizeof(root_offset_bytes)) {
    std::fclose(file); SetError(error, error_size, "root offset read failed"); return false;
  }
  info->file_size = static_cast<std::uint64_t>(file_size);
  info->root_offset = ReadLe32(root_offset_bytes);
  if (info->root_offset > info->file_size || info->file_size - info->root_offset < kFolderHeaderSize ||
      std::fseek(file, static_cast<long>(info->root_offset), SEEK_SET) != 0) {
    std::fclose(file); SetError(error, error_size, "invalid root offset"); return false;
  }
  std::uint8_t header[kFolderHeaderSize];
  if (std::fread(header, 1, sizeof(header), file) != sizeof(header)) {
    std::fclose(file); SetError(error, error_size, "folder header read failed"); return false;
  }
  info->header_size = ReadLe32(header);
  info->entry_count = ReadLe32(header + 4);
  info->entry_record_size = ReadLe32(header + 8);
  const std::uint64_t expected_size = kFolderHeaderSize +
      static_cast<std::uint64_t>(info->entry_count) * info->entry_record_size;
  if (info->entry_record_size != kReleasePackageEntryRecordSize ||
      info->header_size != expected_size || expected_size > info->file_size - info->root_offset) {
    std::fclose(file); SetError(error, error_size, "unsupported package folder layout"); return false;
  }
  if (info->entry_count != 0) {
    std::uint8_t entry[kReleasePackageEntryRecordSize];
    if (std::fread(entry, 1, sizeof(entry), file) != sizeof(entry)) {
      std::fclose(file); SetError(error, error_size, "entry read failed"); return false;
    }
    std::memcpy(info->first_entry_name, entry + 8 + 63, sizeof(info->first_entry_name) - 1);
    info->first_entry_name[sizeof(info->first_entry_name) - 1] = '\0';
  }
  std::fclose(file);
  return true;
}
}  // namespace srhd_awa::platform
