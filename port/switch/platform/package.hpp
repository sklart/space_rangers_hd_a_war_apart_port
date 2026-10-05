#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace srhd_awa::package {
#pragma pack(push, 1)
struct PackEntryDisk { uint32_t stored_size, data_size; char upper_name[63], original_name[63]; int32_t kind, kind_copy; uint32_t flags, unknown92, target_offset, legacy_child_folder; };
#pragma pack(pop)
static_assert(sizeof(PackEntryDisk) == 158);
struct Entry { uint32_t stored_size{}, data_size{}, target_offset{}; int32_t kind{}, kind_copy{}; uint32_t flags{}; std::string upper_name, original_name; std::unique_ptr<struct Folder> child; };
struct Folder { std::vector<Entry> entries; };
struct Summary { uint32_t folders{}, files{}, entries{}, max_depth{}; uint64_t tree_hash{}; std::vector<std::string> paths; };
class Package {
 public:
  bool Open(const std::string& path, std::string* error);
  const Entry* Resolve(const std::string& path) const;
  uint32_t OpenEntryByPath(const std::string& path, std::string* error);
  bool SeekEntry(uint32_t handle, uint64_t position, std::string* error);
  bool SeekEntry(uint32_t handle, uint32_t offset, uint32_t origin, std::string* error);
  size_t ReadEntry(uint32_t handle, void* destination, size_t bytes, std::string* error);
  uint64_t GetEntryPosition(uint32_t handle) const;
  uint64_t GetEntrySize(uint32_t handle) const;
  void CloseEntry(uint32_t handle);
  bool ReadPayload(const Entry& entry, std::vector<uint8_t>* out, std::string* error);
  Summary Summarize() const;
 private:
  struct OpenEntry { const Entry* entry{}; uint64_t position{}; bool open{}; bool decoded{}; std::vector<uint8_t> payload; };
  bool LoadFolder(uint32_t offset, Folder* folder, uint32_t depth, std::vector<uint32_t>& active_offsets, std::string* error);
  std::unique_ptr<Folder> root_; std::string path_; uint64_t size_{}; std::vector<OpenEntry> open_entries_;
};
}  // namespace srhd_awa::package
