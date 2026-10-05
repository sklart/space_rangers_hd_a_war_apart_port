#include "ec_file_adapter.hpp"
#include "game_path.hpp"
#include "package.hpp"
#include "user_root.hpp"
#include "units/EC_HsFile.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <unordered_map>

namespace {
EC_HsFile::TPackCollectionEC g_collection;
pas::CriticalSection g_lock;

struct OpenSlot {
  FILE* loose_file{};
  uint32_t package_handle{};
  uint64_t position{};
  uint64_t size{};
  bool writable{};
};
struct PackState {
  std::unique_ptr<srhd_awa::package::Package> package;
  std::array<OpenSlot, 16> slots{};
  bool opened{};
};
std::unordered_map<EC_HsFile::TPackFileEC*, PackState> g_pack_states;

PackState* State(EC_HsFile::TPackFileEC* pack) {
  const auto it = g_pack_states.find(pack);
  return it == g_pack_states.end() ? nullptr : &it->second;
}

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

void TPackFileEC_Create(TPackFileEC* self) {
  self->PackageHandle = 0xffffffffu;
  self->UseLooseFiles = false;
  self->RootFolder = nullptr;
  self->RootSubtreeOffset = 0;
  self->NextPack = nullptr;
  self->PrevPack = nullptr;
  self->CollectionIndex = -1;
  for (std::uint32_t i = 0; i < 16; ++i) self->OpenSlots[i].IsAvailable = true;
  g_pack_states.emplace(self, PackState{});
}

void TPackFileEC_Destroy(TPackFileEC* self) {
  self->CloseForDestroy();
  g_pack_states.erase(self);
}

void TPackFileEC::p_destroy() { TPackFileEC_Destroy(this); }

void TPackFileEC::SetPackagePath(pas::AnsiString path) { PackagePath = path; }

std::int32_t TPackFileEC::FindFreeOpenSlotIndex() {
  for (std::int32_t i = 0; i < 16; ++i) if (OpenSlots[i].IsAvailable) return i;
  return -1;
}

std::uint8_t TPackFileEC::Open() {
  Close();
  auto* state = State(this);
  if (!state) return false;
  if (!UseLooseFiles) {
    const std::string resolved = srhd_awa::platform::game_path::Resolve(PackagePath.c_str());
    const std::string path = resolved.empty() ? PackagePath.c_str() : resolved;
    if (!state->package) state->package = std::make_unique<srhd_awa::package::Package>();
    std::string error;
    if (path.empty() || !state->package->Open(path, &error)) {
      if (TraceEnabled()) std::fprintf(stderr, "[EC_FILE] package open failed path=%s error=%s\n", path.c_str(), error.c_str());
      return false;
    }
  }
  state->opened = true;
  PackageHandle = UseLooseFiles ? 0xffffffffu : 0;
  return true;
}

std::uint8_t TPackFileEC::Close() {
  auto* state = State(this);
  if (!state || !state->opened) return false;
  CloseAllOpenEntrySlots();
  state->package.reset();
  state->opened = false;
  PackageHandle = 0xffffffffu;
  return true;
}

std::uint8_t TPackFileEC::CloseForDestroy() { return Close(); }

void TPackFileEC::CloseAllOpenEntrySlots() {
  for (std::uint32_t i = 0; i < 16; ++i) if (!OpenSlots[i].IsAvailable) CloseEntrySlot(i);
}

std::int32_t TPackFileEC::OpenEntryByPath(pas::AnsiString path, std::uint32_t) {
  auto* state = State(this);
  const auto slot = FindFreeOpenSlotIndex();
  if (!state || !state->opened || slot < 0) return -1;
  OpenSlot& open = state->slots[slot];
  if (UseLooseFiles) {
    std::string file_path = srhd_awa::platform::user_root::ResolveConfigPath(path.c_str());
    if (file_path.empty()) file_path = srhd_awa::platform::game_path::Resolve(path.c_str());
    if (file_path.empty() || !(open.loose_file = std::fopen(file_path.c_str(), "rb"))) return -1;
    if (std::fseek(open.loose_file, 0, SEEK_END) != 0 || (open.size = std::ftell(open.loose_file)) == UINT64_MAX || std::fseek(open.loose_file, 0, SEEK_SET) != 0) {
      std::fclose(open.loose_file); open = {}; return -1;
    }
  } else {
    std::string error;
    open.package_handle = state->package->OpenEntryByPath(path.c_str(), &error);
    if (!open.package_handle) return -1;
    open.size = state->package->GetEntrySize(open.package_handle);
  }
  open.position = 0;
  OpenSlots[slot].IsAvailable = false;
  return slot;
}

std::uint8_t TPackFileEC::CloseEntrySlot(std::uint32_t slot) {
  auto* state = State(this);
  if (!state || slot >= 16 || OpenSlots[slot].IsAvailable) return false;
  auto& open = state->slots[slot];
  if (open.loose_file) std::fclose(open.loose_file);
  if (open.package_handle && state->package) state->package->CloseEntry(open.package_handle);
  open = {};
  OpenSlots[slot].IsAvailable = true;
  return true;
}

std::uint8_t TPackFileEC::ReadEntrySlot(std::uint32_t slot, void* buffer, std::uint32_t bytes) {
  auto* state = State(this);
  if (!state || slot >= 16 || OpenSlots[slot].IsAvailable) return false;
  auto& open = state->slots[slot];
  if (open.loose_file) {
    const size_t read = std::fread(buffer, 1, bytes, open.loose_file);
    open.position += read;
    return read == bytes;
  }
  std::string error;
  const size_t read = state->package->ReadEntry(open.package_handle, buffer, bytes, &error);
  open.position += read;
  return read == bytes;
}

std::uint8_t TPackFileEC::WriteEntrySlot(std::uint32_t slot, void* buffer, std::uint32_t bytes) {
  auto* state = State(this);
  if (!state || slot >= 16 || OpenSlots[slot].IsAvailable) return false;
  auto& open = state->slots[slot];
  if (!open.loose_file || !open.writable) return false;
  const size_t written = std::fwrite(buffer, 1, bytes, open.loose_file);
  open.position += written;
  if (open.position > open.size) open.size = open.position;
  return written == bytes;
}

std::uint8_t TPackFileEC::SeekEntrySlot(std::uint32_t slot, std::uint32_t offset, std::int32_t origin) {
  auto* state = State(this);
  if (!state || slot >= 16 || OpenSlots[slot].IsAvailable) return false;
  auto& open = state->slots[slot];
  uint64_t position = origin == 0 ? offset : origin == 1 ? open.position + offset : offset <= open.size ? open.size - offset : UINT64_MAX;
  if (position > open.size) return false;
  if (open.loose_file) {
    if (std::fseek(open.loose_file, static_cast<long>(position), SEEK_SET) != 0) return false;
  } else {
    std::string error;
    if (!state->package->SeekEntry(open.package_handle, position, &error)) return false;
  }
  open.position = position;
  return true;
}

std::uint32_t TPackFileEC::GetEntrySlotPosition(std::uint32_t slot) {
  auto* state = State(this);
  return !state || slot >= 16 || OpenSlots[slot].IsAvailable || state->slots[slot].position > UINT32_MAX ? 0xffffffffu : static_cast<uint32_t>(state->slots[slot].position);
}

std::uint32_t TPackFileEC::GetEntrySlotSize(std::uint32_t slot) {
  auto* state = State(this);
  return !state || slot >= 16 || OpenSlots[slot].IsAvailable || state->slots[slot].size > UINT32_MAX ? 0xffffffffu : static_cast<uint32_t>(state->slots[slot].size);
}

void TPackCollectionEC_Create(TPackCollectionEC* self) {
  self->FirstPack = nullptr;
  self->LastPack = nullptr;
  self->NameToPackIndexHash = nullptr;
  self->UseFastNameIndex = false;
  for (std::int32_t i = 0; i < 128; ++i) self->PackByIndex[i] = nullptr;
}

void TPackCollectionEC_Destroy(TPackCollectionEC* self) { self->Clear(true); }

void TPackCollectionEC::p_destroy() { TPackCollectionEC_Destroy(this); }

void TPackCollectionEC::Clear(std::uint8_t free_packs) {
  CloseAllPackages();
  while (FirstPack) {
    TPackFileEC* pack = FirstPack;
    FirstPack = pack->NextPack;
    if (free_packs) pas::free(pack);
  }
  LastPack = nullptr;
  for (std::int32_t i = 0; i < 128; ++i) PackByIndex[i] = nullptr;
}

void Reindex(TPackCollectionEC* collection) {
  for (std::int32_t i = 0; i < 128; ++i) collection->PackByIndex[i] = nullptr;
  std::int32_t index = 0;
  for (auto* pack = collection->FirstPack; pack; pack = pack->NextPack) {
    if (index >= 128) break;
    pack->CollectionIndex = index;
    collection->PackByIndex[index++] = pack;
  }
}

void TPackCollectionEC::AddPackToFront(TPackFileEC* pack) {
  if (!pack) return;
  pack->PrevPack = nullptr;
  pack->NextPack = FirstPack;
  if (FirstPack) FirstPack->PrevPack = pack; else LastPack = pack;
  FirstPack = pack;
  Reindex(this);
}

void TPackCollectionEC::AddPackToBack(TPackFileEC* pack) {
  if (!pack) return;
  pack->NextPack = nullptr;
  pack->PrevPack = LastPack;
  if (LastPack) LastPack->NextPack = pack; else FirstPack = pack;
  LastPack = pack;
  Reindex(this);
}

void TPackCollectionEC::RemovePack(TPackFileEC* pack, std::uint8_t free_pack) {
  if (!pack) return;
  if (pack->PrevPack) pack->PrevPack->NextPack = pack->NextPack;
  else if (FirstPack == pack) FirstPack = pack->NextPack;
  if (pack->NextPack) pack->NextPack->PrevPack = pack->PrevPack;
  else if (LastPack == pack) LastPack = pack->PrevPack;
  Reindex(this);
  if (free_pack) pas::free(pack);
}

std::uint8_t TPackCollectionEC::OpenAllPackages() {
  for (auto* pack = FirstPack; pack; pack = pack->NextPack) {
    if (!pack->Open()) { CloseAllPackages(); return false; }
  }
  return true;
}

std::uint8_t TPackCollectionEC::CloseAllPackages() {
  for (auto* pack = FirstPack; pack; pack = pack->NextPack) pack->Close();
  return true;
}

TPackFileEC* TPackCollectionEC::GetPackByIndex(std::int32_t index) {
  return index < 0 || index >= 128 ? nullptr : PackByIndex[index];
}
std::int32_t TPackCollectionEC::OpenEntryByPathAcrossPackages(
    pas::AnsiString path, std::uint32_t, std::uint8_t first_package_only) {
  for (auto* pack = FirstPack; pack; pack = pack->NextPack) {
    const auto slot = pack->OpenEntryByPath(path, 0);
    if (slot >= 0) {
      const auto handle = pack->CollectionIndex * 16 + slot;
      if (TraceEnabled()) std::fprintf(stderr, "[EC_FILE] open path=%s handle=%d\n", path.c_str(), handle);
      return handle;
    }
    if (first_package_only) break;
  }
  return -1;
}
std::int32_t TPackCollectionEC::CreateLooseFile(pas::WideString path) {
  const std::string resolved = srhd_awa::platform::user_root::ResolveConfigPath(static_cast<pas::AnsiString>(path).c_str());
  if (resolved.empty()) return -1;
  for (auto* pack = FirstPack; pack; pack = pack->NextPack) {
    if (!pack->UseLooseFiles) continue;
    auto* state = State(pack);
    const auto slot = pack->FindFreeOpenSlotIndex();
    if (!state || slot < 0) return -1;
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(resolved).parent_path(), error);
    if (error) return -1;
    auto& open = state->slots[slot];
    open.loose_file = std::fopen(resolved.c_str(), "w+b");
    if (!open.loose_file) return -1;
    open.position = 0;
    open.size = 0;
    open.writable = true;
    pack->OpenSlots[slot].IsAvailable = false;
    return pack->CollectionIndex * 16 + slot;
  }
  return -1;
}
std::uint8_t TPackCollectionEC::CloseEntryHandle(std::int32_t handle) {
  if (handle < 0) return false;
  const std::int32_t index = handle / 16;
  auto* pack = GetPackByIndex(index);
  if (!pack) return false;
  Trace("close", handle);
  return pack->CloseEntrySlot(static_cast<uint32_t>(handle % 16));
}
std::uint8_t TPackCollectionEC::ReadEntryHandle(std::int32_t handle, void* buffer, std::uint32_t bytes) {
  if (handle < 0) return false;
  auto* pack = GetPackByIndex(handle / 16);
  if (!pack) return false;
  const bool ok = pack->ReadEntrySlot(static_cast<uint32_t>(handle % 16), buffer, bytes);
  if (ok) Trace("read", handle, bytes);
  return ok;
}
std::uint8_t TPackCollectionEC::WriteEntryHandle(std::int32_t handle, void* buffer, std::uint32_t bytes) {
  if (handle < 0) return false;
  auto* pack = GetPackByIndex(handle / 16);
  return pack && pack->WriteEntrySlot(static_cast<uint32_t>(handle % 16), buffer, bytes);
}
std::uint8_t TPackCollectionEC::SeekEntryHandle(std::int32_t handle, std::uint32_t offset, std::int32_t origin) {
  if (handle < 0) return false;
  auto* pack = GetPackByIndex(handle / 16);
  const bool ok = pack && pack->SeekEntrySlot(static_cast<uint32_t>(handle % 16), offset, origin);
  if (ok) Trace("seek", handle, offset);
  return ok;
}
std::uint32_t TPackCollectionEC::GetEntryHandlePosition(std::int32_t handle) {
  if (handle < 0) return 0xffffffffu;
  auto* pack = GetPackByIndex(handle / 16);
  return pack ? pack->GetEntrySlotPosition(static_cast<uint32_t>(handle % 16)) : 0xffffffffu;
}
std::uint32_t TPackCollectionEC::GetEntryHandleSize(std::int32_t handle) {
  if (handle < 0) return 0xffffffffu;
  auto* pack = GetPackByIndex(handle / 16);
  return pack ? pack->GetEntrySlotSize(static_cast<uint32_t>(handle % 16)) : 0xffffffffu;
}
}

namespace srhd_awa::platform::ec_file {
bool OpenPackage(const std::string& path, std::string* error) {
  return OpenPackages({path}, error);
}
void SetGameRoot(const std::string& game_root) { srhd_awa::platform::game_path::SetRoot(game_root); }
void SetUserRoot(const std::string& user_root) {
  srhd_awa::platform::user_root::SetRoot(user_root);
  srhd_awa::platform::user_root::EnsureLayout();
}
bool OpenPackages(const std::vector<std::string>& paths, std::string* error) {
  ClosePackage();
  if (paths.empty()) {
    *error = "empty package collection";
    return false;
  }
  EC_HsFile::TPackCollectionEC_Create(&g_collection);
  for (const auto& path : paths) {
    auto* pack = pas::construct_call<EC_HsFile::TPackFileEC>(EC_HsFile::TPackFileEC_Create);
    pack->SetPackagePath(pas::AnsiString(path.c_str()));
    g_collection.AddPackToBack(pack);
  }
  if (!g_collection.OpenAllPackages()) {
    ClosePackage();
    *error = "package open failed";
    return false;
  }
  g_lock.p_create();
  EC_HsFile::PackageCollection = &g_collection;
  EC_HsFile::PackageFileLock = &g_lock;
  return true;
}
void ClosePackage() {
  if (EC_HsFile::PackageCollection == &g_collection) g_collection.Clear(true);
  EC_HsFile::PackageCollection = nullptr;
  EC_HsFile::PackageFileLock = nullptr;
}
}
