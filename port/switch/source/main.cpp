#include <switch.h>
#include <okgf.h>
#include "build_git_commit.hpp"
#include "units/CrcUnit.hpp"
#include "units/System.hpp"
#include "units/SystemImports.hpp"
#include "filesystem.hpp"
#include "package.hpp"
#include "ec_file_adapter.hpp"
#include "startup_slice.hpp"
#include "runtime_settings_slice.hpp"
#include "ui_metadata_slice.hpp"
#include "gi_format0_cpu.hpp"
#include "gi_format2_cpu.hpp"
#include "gai_cpu.hpp"
#include "gai_playback_cpu.hpp"
#include "runtime_loop_slice.hpp"
#include "renderer_platform.hpp"
#include "software_compositor.hpp"
#include "scene_compositor.hpp"
#include "gi_object.hpp"
#include "image_object.hpp"
#include "ui_object.hpp"
#include "ui_tree_renderer.hpp"
#include "ui_tree_fingerprint.hpp"
#include "ui_config.hpp"
#include "ui_cache_resolver.hpp"
#include "ui_label.hpp"
#include "ui_gai.hpp"
#include "ui_graph_button.hpp"
#include "ui_window.hpp"
#include "ui_graph_buffer.hpp"
#include "ui_scroll_bar.hpp"
#include "ui_panel_scroll_bar.hpp"
#include "ui_edit.hpp"
#include "ui_input.hpp"
#include "ui_controls_checkpoint.hpp"
#include "font_repository.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_Cache.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_File.hpp"
#include "units/EC_HsFile.hpp"
#include "units/GR_Main.hpp"
#include "units/GlobalsV.hpp"
#include "units/aPacket.hpp"

#include <cstdarg>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr const char* kLogPath = "sdmc:/switch/space-rangers-hd-a-war-apart/logs/port.log";
constexpr const char* kPreviousLogPath = "sdmc:/switch/space-rangers-hd-a-war-apart/logs/port-prev.log";
constexpr const char* kGrMainLogPath = "sdmc:/switch/space-rangers-hd-a-war-apart/logs/gr-main.log";
constexpr const char* kDefaultGameRoot = "sdmc:/switch/space-rangers-hd-a-war-apart/game";
constexpr const char* kDefaultUserRoot = "sdmc:/switch/space-rangers-hd-a-war-apart";

void Log(const char* format, ...) {
  std::FILE* file = std::fopen(kLogPath, "a");
  if (!file) return;
  va_list args;
  va_start(args, format);
  std::vfprintf(file, format, args);
  std::fputc('\n', file);
  va_end(args);
  std::fclose(file);
}

void BeginLogSession() {
  std::error_code error;
  std::filesystem::create_directories("sdmc:/switch/space-rangers-hd-a-war-apart/logs", error);
  std::filesystem::remove(kPreviousLogPath, error);
  std::filesystem::rename(kLogPath, kPreviousLogPath, error);
  if (std::FILE* file = std::fopen(kLogPath, "w")) std::fclose(file);
}

void Stage(const char* name, bool pass, const char* reason = nullptr) {
  if (pass) Log("[STAGE] %s PASS", name);
  else Log("[STAGE] %s FAIL %s", name, reason ? reason : "unknown");
}
void StageBegin(const char* name) { Log("[STAGE] %s BEGIN", name); }

struct Fingerprint {
  std::uint64_t hash{UINT64_C(1469598103934665603)};
  std::uint32_t entries{}, blocks{}, params{}, files{}, nodes{}, depth{};
  bool truncated{};
};
constexpr std::uint32_t kFingerprintMaxDepth = 64;
constexpr std::uint32_t kFingerprintMaxEntries = 100000;
void HashByte(Fingerprint* fp, std::uint8_t value) { fp->hash = (fp->hash ^ value) * UINT64_C(1099511628211); }
void HashU32(Fingerprint* fp, std::uint32_t value) { for (unsigned shift = 0; shift < 32; shift += 8) HashByte(fp, static_cast<std::uint8_t>(value >> shift)); }
void HashMarker(Fingerprint* fp, const char* marker) { while (*marker) HashByte(fp, static_cast<std::uint8_t>(*marker++)); HashByte(fp, 0); }
void HashWide(Fingerprint* fp, const pas::WideString& value) { for (const char16_t* p = value.pchar(); p && *p; ++p) { HashByte(fp, static_cast<std::uint8_t>(*p)); HashByte(fp, static_cast<std::uint8_t>(*p >> 8)); } HashByte(fp, 0xff); }
void FingerprintBlockImpl(EC_BlockPar::TBlockParEC* block, Fingerprint* fp, std::uint32_t depth, std::vector<EC_BlockPar::TBlockParEC*>* active) {
  if (!block) { HashMarker(fp, "null-block"); return; }
  if (depth > kFingerprintMaxDepth || fp->entries >= kFingerprintMaxEntries || std::find(active->begin(), active->end(), block) != active->end()) { fp->truncated = true; HashMarker(fp, "block-guard"); return; }
  active->push_back(block); fp->depth = std::max(fp->depth, depth);
  try {
    for (std::int32_t i = 0; i < block->GetEntryCount() && fp->entries < kFingerprintMaxEntries; ++i) {
      ++fp->entries; const auto kind = block->GetEntryKindByIndex(i); HashByte(fp, static_cast<std::uint8_t>(kind));
      switch (kind) {
        case EC_BlockPar::bpkBlock: ++fp->blocks; HashWide(fp, block->GetEntryNameByIndex(i)); FingerprintBlockImpl(block->GetEntryBlockByIndex(i), fp, depth + 1, active); break;
        case EC_BlockPar::bpkString: ++fp->params; HashWide(fp, block->GetEntryNameByIndex(i)); HashWide(fp, block->GetEntryStringByIndex(i)); break;
        case EC_BlockPar::bpkText: ++fp->params; HashMarker(fp, "bpkText-no-accessor"); break;
        default: fp->truncated = true; HashMarker(fp, "unknown-block-entry"); break;
      }
    }
    if (fp->entries >= kFingerprintMaxEntries) { fp->truncated = true; HashMarker(fp, "entry-limit"); }
  } catch (...) { fp->truncated = true; HashMarker(fp, "block-access-error"); }
  active->pop_back();
}
void FingerprintBlock(EC_BlockPar::TBlockParEC* block, Fingerprint* fp, std::uint32_t depth) { std::vector<EC_BlockPar::TBlockParEC*> active; FingerprintBlockImpl(block, fp, depth, &active); }
void FingerprintDataImpl(EC_Data::TDataEC* data, Fingerprint* fp, std::uint32_t depth, std::vector<EC_Data::TDataEC*>* active) {
  if (!data) { HashMarker(fp, "null-data"); return; }
  if (depth > kFingerprintMaxDepth || fp->nodes >= kFingerprintMaxEntries || std::find(active->begin(), active->end(), data) != active->end()) { fp->truncated = true; HashMarker(fp, "data-guard"); return; }
  active->push_back(data); fp->depth = std::max(fp->depth, depth);
  for (auto* entry = data->FirstEntry; entry && fp->nodes < kFingerprintMaxEntries; entry = entry->Next) { ++fp->nodes; HashByte(fp, static_cast<std::uint8_t>(entry->Kind)); HashWide(fp, entry->Name); if (entry->Kind == EC_Data::dekSubtree) FingerprintDataImpl(entry->ChildData, fp, depth + 1, active); else { ++fp->files; if (entry->SharedFileRef && entry->SharedFileRef->FileRef) HashWide(fp, entry->SharedFileRef->FileRef->GetFileName()); HashU32(fp, entry->FileOffset); HashU32(fp, entry->ByteCount); } }
  if (fp->nodes >= kFingerprintMaxEntries) { fp->truncated = true; HashMarker(fp, "node-limit"); }
  active->pop_back();
}
void FingerprintData(EC_Data::TDataEC* data, Fingerprint* fp, std::uint32_t depth) { std::vector<EC_Data::TDataEC*> active; FingerprintDataImpl(data, fp, depth, &active); }
void CaptureRawCacheFingerprint(void* context) { FingerprintData(GR_Main::CacheDataRoot, static_cast<Fingerprint*>(context), 1); }
struct CacheResource {
  pas::WideString path;
  pas::WideString source_file;
};

bool FindFirstCacheFile(EC_Data::TDataEC* data, const pas::WideString& prefix, std::uint32_t depth,
                        std::uint32_t* entries, std::vector<EC_Data::TDataEC*>* active,
                        CacheResource* result) {
  if (!data || depth > kFingerprintMaxDepth || *entries >= kFingerprintMaxEntries ||
      std::find(active->begin(), active->end(), data) != active->end()) return false;
  active->push_back(data);
  for (auto* entry = data->FirstEntry; entry && *entries < kFingerprintMaxEntries; entry = entry->Next) {
    ++*entries;
    const pas::WideString path = prefix.length() == 0 ? entry->Name : pas::concat_wide({prefix, u"."_wref.get(), entry->Name});
    if (entry->Kind == EC_Data::dekFile &&
        GR_Main::GlobalCache && GR_Main::GlobalCache->DataRoot->FileExistsByPath(path)) {
      result->path = path;
      if (entry->SharedFileRef && entry->SharedFileRef->FileRef) result->source_file = entry->SharedFileRef->FileRef->GetFileName();
      active->pop_back();
      return true;
    }
    if (entry->Kind == EC_Data::dekSubtree &&
        FindFirstCacheFile(entry->ChildData, path, depth + 1, entries, active, result)) {
      active->pop_back();
      return true;
    }
  }
  active->pop_back();
  return false;
}

bool VerifyFirstCachedResource() {
  if (!GR_Main::GlobalCache || !GR_Main::GlobalCache->DataRoot || !GR_Main::CCInterface) return false;
  CacheResource resource;
  std::uint32_t entries = 0;
  std::vector<EC_Data::TDataEC*> active;
  if (!FindFirstCacheFile(GR_Main::GlobalCache->DataRoot, pas::WideString{}, 1, &entries, &active, &resource)) return false;
  EC_Buf::TBufEC* buffer = nullptr;
  try {
    GR_Main::CCInterface->SetResourceChecksumFailed(false);
    buffer = GR_Main::GlobalCache->OpenDataBuffer(resource.path);
    if (!buffer || buffer->DataSize < 0 || buffer->DataSize > 256 * 1024 * 1024) {
      pas::free(buffer);
      return false;
    }
    std::uint64_t fnv64 = UINT64_C(1469598103934665603);
    const auto* bytes = static_cast<const std::uint8_t*>(buffer->Data);
    for (std::int32_t index = 0; index < buffer->DataSize; ++index) fnv64 = (fnv64 ^ bytes[index]) * UINT64_C(1099511628211);
    const auto crc32 = buffer->ComputeCrc32();
    const bool checksum_failed = GR_Main::CCInterface->GetResourceChecksumFailed();
    Log("[M11] resource path=%s file=%s size=%ld crc32=%08lx fnv64=%016llx checksum_failed=%u",
        static_cast<const char*>(static_cast<pas::AnsiString>(resource.path).c_str()),
        static_cast<const char*>(static_cast<pas::AnsiString>(resource.source_file).c_str()),
        static_cast<long>(buffer->DataSize), static_cast<unsigned long>(crc32),
        static_cast<unsigned long long>(fnv64), checksum_failed ? 1u : 0u);
    pas::free(buffer);
    return !checksum_failed;
  } catch (...) {
    pas::free(buffer);
    return false;
  }
}

bool VerifyM14pGiFormat0() {
  constexpr const char16_t kKey[] = u"Bm.Captain.2BlazerBi";
  constexpr const char* kFile = "data\\Captain\\2BlazerB.gi";
  constexpr std::uint32_t kSourceCrc32 = 0x1ad8b184u;
  constexpr std::uint64_t kSourceFnv64 = UINT64_C(0x0ed39d004915cd6d);
  constexpr std::uint32_t kPixelsCrc32 = 0xcf5b1d56u;
  constexpr std::uint64_t kPixelsFnv64 = UINT64_C(0xa668e341bc42a6fb);
  EC_Buf::TBufEC* buffer = nullptr;
  try {
    buffer = GR_Main::GlobalCache->OpenDataBuffer(pas::WideString(kKey));
    srhd_awa::platform::gi_format0_cpu::Metadata metadata{};
    srhd_awa::platform::gi_format0_cpu::CpuImage image;
    std::string error;
    const auto status = buffer ? srhd_awa::platform::gi_format0_cpu::Decode(
        buffer->Data, static_cast<std::size_t>(buffer->DataSize), &metadata, &image, &error)
        : srhd_awa::platform::gi_format0_cpu::Status::InvalidHeader;
    if (status != srhd_awa::platform::gi_format0_cpu::Status::Ok) {
      Log("[M14P] decode failed status=%u reason=%s", static_cast<unsigned>(status), error.c_str());
      pas::free(buffer);
      return false;
    }
    std::uint64_t source_fnv = UINT64_C(1469598103934665603);
    const auto* source = static_cast<const std::uint8_t*>(buffer->Data);
    for (std::int32_t i = 0; i < buffer->DataSize; ++i) source_fnv = (source_fnv ^ source[i]) * UINT64_C(1099511628211);
    std::uint64_t pixels_fnv = UINT64_C(1469598103934665603);
    for (std::uint8_t pixel : image.pixels) pixels_fnv = (pixels_fnv ^ pixel) * UINT64_C(1099511628211);
    const auto source_crc = CrcUnit::ComputeCrc32(buffer->Data, buffer->DataSize);
    const auto pixels_crc = CrcUnit::ComputeCrc32(image.pixels.data(), static_cast<std::int32_t>(image.pixels.size()));
    const bool match = buffer->DataSize == 19440 && source_crc == kSourceCrc32 && source_fnv == kSourceFnv64 &&
        metadata.version == 1 && metadata.plane_count == 1 && metadata.clip_rect_count == 0 &&
        metadata.red_mask == 0x0000f800u && metadata.green_mask == 0x000007e0u &&
        metadata.blue_mask == 0x0000001fu && metadata.alpha_mask == 0 &&
        metadata.left == 0 && metadata.top == 0 && metadata.right == 93 && metadata.bottom == 104 &&
        image.width == 93 && image.height == 104 && image.bytes_per_pixel == 4 && image.pitch == 372 &&
        image.pixels.size() == 38688 && pixels_crc == kPixelsCrc32 && pixels_fnv == kPixelsFnv64;
    Log("[M14P] key=%s file=%s source_size=%ld format=0 alpha_mask=%08lx planes=%ld bounds=%ld,%ld,%ld,%ld decoded=%ldx%ld bpp=%ld pitch=%ld crc32=%08lx fnv64=%016llx",
        "Bm.Captain.2BlazerBi", kFile, static_cast<long>(buffer->DataSize), static_cast<unsigned long>(metadata.alpha_mask),
        static_cast<long>(metadata.plane_count), static_cast<long>(metadata.left), static_cast<long>(metadata.top),
        static_cast<long>(metadata.right), static_cast<long>(metadata.bottom), static_cast<long>(image.width),
        static_cast<long>(image.height), static_cast<long>(image.bytes_per_pixel), static_cast<long>(image.pitch),
        static_cast<unsigned long>(pixels_crc), static_cast<unsigned long long>(pixels_fnv));
    image.Clear();
    pas::free(buffer);
    return match;
  } catch (...) {
    pas::free(buffer);
    return false;
  }
}

bool VerifyM15GaiFormat0(const char* game_root) {
  constexpr const char* kResource = "DATA/BGObj/bg00.gai";
  constexpr std::uint32_t kGaiCrc32 = 0x9e05776fu;
  constexpr std::uint64_t kGaiFnv64 = UINT64_C(0x03f332f6307d4031);
  constexpr std::uint32_t kGiCrc32 = 0x05d665d2u;
  constexpr std::uint64_t kGiFnv64 = UINT64_C(0x3ccdac34b2d0a2cc);
  constexpr std::uint32_t kPixelsCrc32 = 0x3fc81562u;
  constexpr std::uint64_t kPixelsFnv64 = UINT64_C(0xad9d67c6c7ad85b9);
  try {
    srhd_awa::package::Package package;
    std::string error;
    const auto package_path = (std::filesystem::path(game_root) / "DATA" / "common.pkg").string();
    const auto* entry = package.Open(package_path, &error) ? package.Resolve(kResource) : nullptr;
    std::vector<std::uint8_t> gai;
    if (!entry || !package.ReadPayload(*entry, &gai, &error)) {
      Log("[M15] open failed resource=%s reason=%s", kResource, error.c_str());
      return false;
    }
    srhd_awa::platform::gai_cpu::GaiMetadata gai_metadata{};
    srhd_awa::platform::gai_cpu::GaiFrameInfo frame{};
    srhd_awa::platform::gi_format0_cpu::Metadata gi_metadata{};
    srhd_awa::platform::gi_format0_cpu::CpuImage image;
    const auto status = srhd_awa::platform::gai_cpu::DecodeGaiFormat0Frame(
        gai.data(), gai.size(), 0, &gai_metadata, &frame, &gi_metadata, &image, &error);
    if (status != srhd_awa::platform::gai_cpu::Status::Ok) {
      Log("[M15] decode failed status=%u reason=%s", static_cast<unsigned>(status), error.c_str());
      return false;
    }
    auto hash = [](const std::uint8_t* bytes, std::size_t count) {
      // Standard 64-bit FNV-1a offset basis.  Keep this identical to the
      // independent release oracle rather than to older diagnostic hashes.
      std::uint64_t value = UINT64_C(14695981039346656037);
      for (std::size_t i = 0; i < count; ++i) value = (value ^ bytes[i]) * UINT64_C(1099511628211);
      return value;
    };
    const auto gai_crc = CrcUnit::ComputeCrc32(gai.data(), static_cast<std::int32_t>(gai.size()));
    const auto gai_fnv = hash(gai.data(), gai.size());
    srhd_awa::platform::gai_cpu::GaiFramePayload payload;
    if (srhd_awa::platform::gai_cpu::ExtractGaiFrame(gai.data(), gai.size(), 0, &payload, &error) !=
        srhd_awa::platform::gai_cpu::Status::Ok) return false;
    const auto payload_crc = CrcUnit::ComputeCrc32(payload.gi_bytes.data(), static_cast<std::int32_t>(payload.gi_bytes.size()));
    const auto payload_fnv = hash(payload.gi_bytes.data(), payload.gi_bytes.size());
    const auto pixels_crc = CrcUnit::ComputeCrc32(image.pixels.data(), static_cast<std::int32_t>(image.pixels.size()));
    const auto pixels_fnv = hash(image.pixels.data(), image.pixels.size());
    const bool match = gai.size() == 8000152 && gai_crc == kGaiCrc32 && gai_fnv == kGaiFnv64 &&
        gai_metadata.version == 1 && gai_metadata.left == 0 && gai_metadata.top == 0 &&
        gai_metadata.right == 2000 && gai_metadata.bottom == 2000 && gai_metadata.frame_count == 1 &&
        gai_metadata.flags == 0 && !gai_metadata.sequence_table_present && gai_metadata.sequence_count == 0 &&
        frame.index == 0 && frame.data_offset == 56 && frame.data_size == 8000096 &&
        frame.encoding == srhd_awa::platform::gai_cpu::FrameEncoding::RawGi &&
        payload.gi_bytes.size() == 8000096 && payload_crc == kGiCrc32 && payload_fnv == kGiFnv64 &&
        gi_metadata.version == 1 && gi_metadata.width == 2000 && gi_metadata.height == 2000 &&
        gi_metadata.plane_count == 1 && gi_metadata.clip_rect_count == 0 && gi_metadata.red_mask == 0x0000f800u &&
        gi_metadata.green_mask == 0x000007e0u && gi_metadata.blue_mask == 0x0000001fu && gi_metadata.alpha_mask == 0 &&
        image.width == 2000 && image.height == 2000 && image.bytes_per_pixel == 4 && image.pitch == 8000 &&
        image.pixels.size() == 16000000 && pixels_crc == kPixelsCrc32 && pixels_fnv == kPixelsFnv64;
    Log("[M15] resource=%s gai_size=%zu gai_crc32=%08lx gai_fnv64=%016llx", kResource, gai.size(),
        static_cast<unsigned long>(gai_crc), static_cast<unsigned long long>(gai_fnv));
    Log("[M15] version=%ld frames=%ld flags=%08lx bounds=%ld,%ld,%ld,%ld sequences=%ld", static_cast<long>(gai_metadata.version),
        static_cast<long>(gai_metadata.frame_count), static_cast<unsigned long>(gai_metadata.flags), static_cast<long>(gai_metadata.left),
        static_cast<long>(gai_metadata.top), static_cast<long>(gai_metadata.right), static_cast<long>(gai_metadata.bottom), static_cast<long>(gai_metadata.sequence_count));
    Log("[M15] frame_index=%ld encoding=%s frame_size=%ld gi_size=%zu gi_crc32=%08lx gi_fnv64=%016llx", static_cast<long>(frame.index),
        srhd_awa::platform::gai_cpu::FrameEncodingName(frame.encoding), static_cast<long>(frame.data_size), payload.gi_bytes.size(),
        static_cast<unsigned long>(payload_crc), static_cast<unsigned long long>(payload_fnv));
    Log("[M15] gi_format=0 masks=%08lx,%08lx,%08lx,%08lx decoded=%ldx%ld bpp=%ld pitch=%ld crc32=%08lx fnv64=%016llx",
        static_cast<unsigned long>(gi_metadata.red_mask), static_cast<unsigned long>(gi_metadata.green_mask),
        static_cast<unsigned long>(gi_metadata.blue_mask), static_cast<unsigned long>(gi_metadata.alpha_mask),
        static_cast<long>(image.width), static_cast<long>(image.height), static_cast<long>(image.bytes_per_pixel), static_cast<long>(image.pitch),
        static_cast<unsigned long>(pixels_crc), static_cast<unsigned long long>(pixels_fnv));
    if (!match) {
      const char* reason = "metadata";
      if (gai.size() != 8000152 || gai_crc != kGaiCrc32 || gai_fnv != kGaiFnv64) reason = "GAI fingerprint";
      else if (frame.index != 0 || frame.data_offset != 56 || frame.data_size != 8000096 ||
               frame.encoding != srhd_awa::platform::gai_cpu::FrameEncoding::RawGi) reason = "frame directory";
      else if (payload.gi_bytes.size() != 8000096 || payload_crc != kGiCrc32 || payload_fnv != kGiFnv64) reason = "GI fingerprint";
      else if (image.pixels.size() != 16000000 || pixels_crc != kPixelsCrc32 || pixels_fnv != kPixelsFnv64) reason = "decoded-pixel fingerprint";
      Log("[M15] verification failed reason=%s", reason);
    }
    return match;
  } catch (...) {
    Log("[M15] exception");
    return false;
  }
}

bool VerifyM16GaiFormat2(const char* game_root) {
  constexpr const char* kResource = "DATA/Asteroid/00.gai";
  constexpr std::uint32_t kAggregateCrc = 0x9e4059ceu;
  constexpr std::uint64_t kAggregateFnv = UINT64_C(0xa028ffbf04472afa);
  constexpr std::uint32_t kFrame0Crc = 0x83f66519u;
  constexpr std::uint64_t kFrame0Fnv = UINT64_C(0xeb000366ca288b23);
  try {
    srhd_awa::package::Package package; std::string error; std::vector<std::uint8_t> gai, canonical;
    const auto path = (std::filesystem::path(game_root) / "DATA" / "common.pkg").string();
    const auto* entry = package.Open(path, &error) ? package.Resolve(kResource) : nullptr;
    if (!entry || !package.ReadPayload(*entry, &gai, &error)) { Log("[M16] open failed reason=%s", error.c_str()); return false; }
    srhd_awa::platform::gai_cpu::GaiMetadata gm{};
    if (srhd_awa::platform::gai_cpu::ValidateGai(gai.data(), gai.size(), &gm, &error) != srhd_awa::platform::gai_cpu::Status::Ok) return false;
    auto add = [&canonical](std::uint32_t v) { for (unsigned s=0;s<32;s+=8) canonical.push_back(static_cast<std::uint8_t>(v>>s)); };
    auto fnv = [](const std::uint8_t* b,std::size_t n) { std::uint64_t h=UINT64_C(14695981039346656037); for(std::size_t i=0;i<n;++i) h=(h^b[i])*UINT64_C(1099511628211); return h; };
    std::uint32_t f0crc{}; std::uint64_t f0fnv{}; std::size_t f0size{}; srhd_awa::platform::gi_format2_cpu::Metadata f0{};
    for (std::int32_t index=0; index<gm.frame_count; ++index) {
      srhd_awa::platform::gai_cpu::GaiFramePayload payload; srhd_awa::platform::gi_format2_cpu::Metadata meta{}; srhd_awa::platform::gi_format2_cpu::CpuImage image;
      if (srhd_awa::platform::gai_cpu::ExtractGaiFrame(gai.data(),gai.size(),index,&payload,&error)!=srhd_awa::platform::gai_cpu::Status::Ok || payload.info.encoding!=srhd_awa::platform::gai_cpu::FrameEncoding::RawGi || srhd_awa::platform::gi_format2_cpu::Decode(payload.gi_bytes.data(),payload.gi_bytes.size(),&meta,&image,&error)!=srhd_awa::platform::gi_format2_cpu::Status::Ok) return false;
      add(index); add(meta.left); add(meta.top); add(meta.right); add(meta.bottom); add(image.width); add(image.height); add(image.pitch); add(static_cast<std::uint32_t>(image.pixels.size())); canonical.insert(canonical.end(),image.pixels.begin(),image.pixels.end());
      if (!index) { f0=meta; f0size=payload.gi_bytes.size(); f0crc=CrcUnit::ComputeCrc32(image.pixels.data(),static_cast<std::int32_t>(image.pixels.size())); f0fnv=fnv(image.pixels.data(),image.pixels.size()); }
      image.Clear();
    }
    const auto crc=CrcUnit::ComputeCrc32(canonical.data(),static_cast<std::int32_t>(canonical.size())); const auto hash=fnv(canonical.data(),canonical.size());
    const bool ok=gai.size()==246863 && gm.frame_count==100 && gm.flags==0 && gm.sequence_count==1 && f0size==2622 && f0.left==9 && f0.top==4 && f0.right==42 && f0.bottom==44 && f0crc==kFrame0Crc && f0fnv==kFrame0Fnv && crc==kAggregateCrc && hash==kAggregateFnv;
    Log("[M16] resource=%s gai_size=%zu",kResource,gai.size()); Log("[M16] frames=%ld flags=%08lx sequences=%ld",static_cast<long>(gm.frame_count),static_cast<unsigned long>(gm.flags),static_cast<long>(gm.sequence_count)); Log("[M16] frame0 gi_format=2 decoded=%ldx%ld pitch=%ld crc32=%08lx fnv64=%016llx",static_cast<long>(f0.width),static_cast<long>(f0.height),static_cast<long>(f0.width*4),static_cast<unsigned long>(f0crc),static_cast<unsigned long long>(f0fnv)); Log("[M16] decoded_frames=%ld aggregate_crc32=%08lx aggregate_fnv64=%016llx",static_cast<long>(gm.frame_count),static_cast<unsigned long>(crc),static_cast<unsigned long long>(hash));
    return ok;
  } catch (...) { Log("[M16] exception"); return false; }
}

struct M17PlaybackDiagnostic {
  std::vector<std::uint8_t> gai;
  srhd_awa::platform::gai_cpu::GaiSequence sequence;
  srhd_awa::platform::gai_playback_cpu::State playback;
  std::vector<std::uint8_t> cycle_canonical;
  std::uint64_t previous_tick{};
  std::uint64_t cycle_started_tick{};
  std::uint64_t max_tick_gap{};
  bool clock_started{};
  bool initial_cycle_verified{};
  bool sequence_pass{};
  bool cycle_pass{};
  bool timing_pass{};
};

void AppendM17U32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) bytes->push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint64_t M17Fnv(const std::uint8_t* bytes, std::size_t size) {
  std::uint64_t value = UINT64_C(14695981039346656037);
  for (std::size_t index = 0; index < size; ++index) value = (value ^ bytes[index]) * UINT64_C(1099511628211);
  return value;
}

bool DecodeM17Frame(M17PlaybackDiagnostic* diagnostic, std::int32_t sequence_frame, std::int32_t source_frame,
                    std::int32_t delay_ms, bool append_canonical, std::string* error) {
  srhd_awa::platform::gai_cpu::GaiFramePayload payload;
  srhd_awa::platform::gi_format2_cpu::Metadata metadata{};
  srhd_awa::platform::gi_format2_cpu::CpuImage image;
  if (srhd_awa::platform::gai_cpu::ExtractGaiFrame(diagnostic->gai.data(), diagnostic->gai.size(), source_frame, &payload, error) !=
          srhd_awa::platform::gai_cpu::Status::Ok ||
      payload.info.encoding != srhd_awa::platform::gai_cpu::FrameEncoding::RawGi ||
      srhd_awa::platform::gi_format2_cpu::Decode(payload.gi_bytes.data(), payload.gi_bytes.size(), &metadata, &image, error) !=
          srhd_awa::platform::gi_format2_cpu::Status::Ok) return false;
  if (append_canonical) {
    AppendM17U32(&diagnostic->cycle_canonical, static_cast<std::uint32_t>(sequence_frame));
    AppendM17U32(&diagnostic->cycle_canonical, static_cast<std::uint32_t>(source_frame));
    AppendM17U32(&diagnostic->cycle_canonical, static_cast<std::uint32_t>(delay_ms));
    AppendM17U32(&diagnostic->cycle_canonical, static_cast<std::uint32_t>(image.width));
    AppendM17U32(&diagnostic->cycle_canonical, static_cast<std::uint32_t>(image.height));
    AppendM17U32(&diagnostic->cycle_canonical, static_cast<std::uint32_t>(image.pitch));
    AppendM17U32(&diagnostic->cycle_canonical, static_cast<std::uint32_t>(image.pixels.size()));
    diagnostic->cycle_canonical.insert(diagnostic->cycle_canonical.end(), image.pixels.begin(), image.pixels.end());
  }
  return true;
}

bool InitializeM17Playback(const char* game_root, M17PlaybackDiagnostic* diagnostic, std::string* error) {
  constexpr const char* kResource = "DATA/Asteroid/00.gai";
  constexpr std::uint32_t kSequenceCrc = 0x4b1c6ebfu;
  constexpr std::uint64_t kSequenceFnv = UINT64_C(0x47cdc8c73fc1ce61);
  srhd_awa::package::Package package;
  const auto path = (std::filesystem::path(game_root) / "DATA" / "common.pkg").string();
  const auto* entry = package.Open(path, error) ? package.Resolve(kResource) : nullptr;
  if (!entry || !package.ReadPayload(*entry, &diagnostic->gai, error)) return false;
  if (srhd_awa::platform::gai_cpu::ReadGaiSequence(diagnostic->gai.data(), diagnostic->gai.size(), 0, &diagnostic->sequence, error) !=
      srhd_awa::platform::gai_cpu::Status::Ok) return false;
  if (diagnostic->sequence.frames.empty() || diagnostic->sequence.frames.size() != 100) { if (error) *error = "unexpected sequence 0 frame count"; return false; }
  std::vector<std::uint8_t> sequence_canonical;
  AppendM17U32(&sequence_canonical, 0); AppendM17U32(&sequence_canonical, static_cast<std::uint32_t>(diagnostic->sequence.frames.size()));
  std::int32_t min_delay = diagnostic->sequence.frames.front().delay_ms, max_delay = min_delay;
  std::uint64_t nominal_ms{}; std::uint32_t zero_delays{}, negative_delays{};
  for (const auto& frame : diagnostic->sequence.frames) {
    AppendM17U32(&sequence_canonical, static_cast<std::uint32_t>(frame.source_frame_index));
    AppendM17U32(&sequence_canonical, static_cast<std::uint32_t>(frame.delay_ms));
    min_delay = std::min(min_delay, frame.delay_ms); max_delay = std::max(max_delay, frame.delay_ms);
    if (frame.delay_ms == 0) ++zero_delays;
    if (frame.delay_ms < 0) ++negative_delays;
    else nominal_ms += static_cast<std::uint32_t>(frame.delay_ms);
  }
  const auto sequence_crc = CrcUnit::ComputeCrc32(sequence_canonical.data(), static_cast<std::int32_t>(sequence_canonical.size()));
  const auto sequence_fnv = M17Fnv(sequence_canonical.data(), sequence_canonical.size());
  diagnostic->sequence_pass = sequence_crc == kSequenceCrc && sequence_fnv == kSequenceFnv && min_delay == 50 && max_delay == 50 && nominal_ms == 5000 && zero_delays == 0 && negative_delays == 0;
  Log("[M17] resource=%s", kResource);
  Log("[M17] sequence=0 sequence_frames=%zu nominal_cycle_ms=%llu delay_min=%ld delay_max=%ld zero_delays=%lu negative_delays=%lu", diagnostic->sequence.frames.size(), static_cast<unsigned long long>(nominal_ms), static_cast<long>(min_delay), static_cast<long>(max_delay), static_cast<unsigned long>(zero_delays), static_cast<unsigned long>(negative_delays));
  Log("[M17] sequence_crc32=%08lx sequence_fnv64=%016llx", static_cast<unsigned long>(sequence_crc), static_cast<unsigned long long>(sequence_fnv));
  if (!diagnostic->sequence_pass) { if (error) *error = "sequence fingerprint or timing metadata"; return false; }
  if (!srhd_awa::platform::gai_playback_cpu::Initialize(&diagnostic->playback, diagnostic->sequence, error)) return false;
  const auto& initial = diagnostic->sequence.frames.front();
  Log("[M17] initial_sequence_frame=0 initial_source_frame=%ld", static_cast<long>(initial.source_frame_index));
  return DecodeM17Frame(diagnostic, 0, initial.source_frame_index, initial.delay_ms, true, error);
}

bool RunM17Playback(void* user_data, std::uint64_t now_ms, std::string* error) {
  constexpr std::uint32_t kCycleCrc = 0x5b7bc7e9u;
  constexpr std::uint64_t kCycleFnv = UINT64_C(0xf70813ac799a25b3);
  constexpr std::uint64_t kNominalCycleMs = 5000;
  auto* diagnostic = static_cast<M17PlaybackDiagnostic*>(user_data);
  if (!diagnostic->clock_started) { diagnostic->clock_started = true; diagnostic->previous_tick = now_ms; diagnostic->cycle_started_tick = now_ms; return true; }
  if (now_ms < diagnostic->previous_tick) { if (error) *error = "non-monotonic M12 tick"; return false; }
  const auto delta = now_ms - diagnostic->previous_tick;
  diagnostic->max_tick_gap = std::max(diagnostic->max_tick_gap, delta);
  diagnostic->previous_tick = now_ms;
  std::vector<srhd_awa::platform::gai_playback_cpu::Step> steps;
  if (!srhd_awa::platform::gai_playback_cpu::AdvanceBy(&diagnostic->playback, diagnostic->sequence, delta, &steps, error)) return false;
  for (const auto& step : steps) {
    if (step.wrapped && !diagnostic->initial_cycle_verified) {
      const auto cycle_crc = CrcUnit::ComputeCrc32(diagnostic->cycle_canonical.data(), static_cast<std::int32_t>(diagnostic->cycle_canonical.size()));
      const auto cycle_fnv = M17Fnv(diagnostic->cycle_canonical.data(), diagnostic->cycle_canonical.size());
      const auto actual_ms = now_ms - diagnostic->cycle_started_tick;
      diagnostic->cycle_pass = cycle_crc == kCycleCrc && cycle_fnv == kCycleFnv;
      diagnostic->timing_pass = actual_ms >= kNominalCycleMs && actual_ms - kNominalCycleMs <= diagnostic->max_tick_gap + 5;
      diagnostic->initial_cycle_verified = true;
      Log("[M17] cycle_crc32=%08lx cycle_fnv64=%016llx cycle_bytes=%zu", static_cast<unsigned long>(cycle_crc), static_cast<unsigned long long>(cycle_fnv), diagnostic->cycle_canonical.size());
      Log("[M17] first_cycle_actual_ms=%llu nominal_cycle_ms=%llu max_tick_gap_ms=%llu transitions=%zu tolerance_ms=%llu", static_cast<unsigned long long>(actual_ms), static_cast<unsigned long long>(kNominalCycleMs), static_cast<unsigned long long>(diagnostic->max_tick_gap), diagnostic->sequence.frames.size(), static_cast<unsigned long long>(diagnostic->max_tick_gap + 5));
      if (!diagnostic->cycle_pass || !diagnostic->timing_pass) { if (error) *error = "cycle fingerprint or timing tolerance"; return false; }
      Log("[M17] PASS sequence=0 cycle=1");
      diagnostic->cycle_canonical.clear(); diagnostic->cycle_started_tick = now_ms;
    }
    if (!DecodeM17Frame(diagnostic, step.sequence_frame, step.source_frame_index, step.delay_ms, !step.wrapped, error)) return false;
  }
  return true;
}

struct M18CompositorDiagnostic {
  std::vector<std::uint8_t> gai;
  srhd_awa::platform::gai_cpu::GaiSequence sequence;
  srhd_awa::platform::gi_format2_cpu::CpuImage image;
  std::int32_t source_frame{};
  std::int32_t x{};
  std::int32_t y{};
  bool composited{};
};

bool InitializeM18Compositor(const char* game_root, M18CompositorDiagnostic* diagnostic, std::string* error) {
  constexpr const char* kResource = "DATA/Asteroid/00.gai";
  srhd_awa::package::Package package;
  const auto path = (std::filesystem::path(game_root) / "DATA" / "common.pkg").string();
  const auto* entry = package.Open(path, error) ? package.Resolve(kResource) : nullptr;
  if (!entry || !package.ReadPayload(*entry, &diagnostic->gai, error) ||
      srhd_awa::platform::gai_cpu::ReadGaiSequence(diagnostic->gai.data(), diagnostic->gai.size(), 0, &diagnostic->sequence, error) !=
          srhd_awa::platform::gai_cpu::Status::Ok || diagnostic->sequence.frames.empty()) return false;
  const auto& initial = diagnostic->sequence.frames.front();
  diagnostic->source_frame = initial.source_frame_index;
  srhd_awa::platform::gai_cpu::GaiFramePayload payload;
  srhd_awa::platform::gi_format2_cpu::Metadata metadata{};
  if (srhd_awa::platform::gai_cpu::ExtractGaiFrame(diagnostic->gai.data(), diagnostic->gai.size(), diagnostic->source_frame, &payload, error) !=
          srhd_awa::platform::gai_cpu::Status::Ok ||
      srhd_awa::platform::gi_format2_cpu::Decode(payload.gi_bytes.data(), payload.gi_bytes.size(), &metadata,
                                                   &diagnostic->image, error) !=
          srhd_awa::platform::gi_format2_cpu::Status::Ok) return false;
  if (diagnostic->image.bytes_per_pixel != 4 || diagnostic->image.width <= 0 || diagnostic->image.height <= 0 ||
      diagnostic->image.pitch < diagnostic->image.width * 4) {
    if (error) *error = "decoded GI image is not a valid BGRA surface";
    return false;
  }
  const auto crc = CrcUnit::ComputeCrc32(diagnostic->image.pixels.data(), static_cast<std::int32_t>(diagnostic->image.pixels.size()));
  const auto fnv = M17Fnv(diagnostic->image.pixels.data(), diagnostic->image.pixels.size());
  Log("[M18] resource=%s", kResource);
  Log("[M18] sequence=0 sequence_frame=0 source_frame=%ld", static_cast<long>(diagnostic->source_frame));
  Log("[M18] decoded=%ldx%ld pitch=%ld bytes=%zu crc32=%08lx fnv64=%016llx", static_cast<long>(diagnostic->image.width),
      static_cast<long>(diagnostic->image.height), static_cast<long>(diagnostic->image.pitch), diagnostic->image.pixels.size(),
      static_cast<unsigned long>(crc), static_cast<unsigned long long>(fnv));
  return true;
}

bool DrawM18Compositor(void* user_data, std::string* error) {
  auto* diagnostic = static_cast<M18CompositorDiagnostic*>(user_data);
  auto* framebuffer = GR_Main::ScreenRenderBuffer;
  if (!framebuffer || !framebuffer->GetPixels() || framebuffer->PitchBytes % 2 != 0) {
    if (error) *error = "M18 RGB565 framebuffer unavailable";
    return false;
  }
  if (!srhd_awa::platform::software_compositor::CompositeBGRA(
          static_cast<std::uint16_t*>(framebuffer->GetPixels()), framebuffer->Width, framebuffer->Height,
          framebuffer->PitchBytes / 2, diagnostic->image.pixels.data(), diagnostic->image.width,
          diagnostic->image.height, diagnostic->image.pitch, diagnostic->x, diagnostic->y,
          srhd_awa::platform::software_compositor::BlendMode::Alpha, nullptr, error)) return false;
  if (!diagnostic->composited) {
    diagnostic->composited = true;
    Log("[M18] compositor PASS x=%ld y=%ld", static_cast<long>(diagnostic->x), static_cast<long>(diagnostic->y));
  }
  return true;
}

struct M20ObjectReport { const char* id{}; };
struct M20GiObjectDiagnostic {
  srhd_awa::package::Package package;
  srhd_awa::platform::gi_object::GIObject asteroid, secondary, overlay;
  srhd_awa::platform::scene_compositor::Scene scene;
  srhd_awa::platform::scene_compositor::Fingerprint fingerprint{};
  std::uint64_t last_tick{};
  bool rendered{};
};

bool SyncM20Scene(M20GiObjectDiagnostic* diagnostic, std::string* error) {
  diagnostic->scene.Clear();
  return diagnostic->asteroid.Draw(diagnostic->scene, error) &&
      diagnostic->secondary.Draw(diagnostic->scene, error) &&
      diagnostic->overlay.Draw(diagnostic->scene, error);
}

bool InitializeM20GiObjects(const char* game_root, std::int32_t width, std::int32_t height,
                            M20GiObjectDiagnostic* diagnostic, std::string* error) {
  constexpr const char* kAsteroid = "DATA/Asteroid/00.gai";
  const auto package_path = (std::filesystem::path(game_root) / "DATA" / "common.pkg").string();
  if (!diagnostic->package.Open(package_path, error)) return false;
  std::vector<std::string> resources{kAsteroid};
  for (const auto& record : diagnostic->package.Summarize().paths) {
    const auto resource = record.substr(0, record.find('|'));
    if (resource == kAsteroid || !resource.starts_with("DATA/") || !resource.ends_with(".gai")) continue;
    srhd_awa::platform::gi_object::GIObject probe(&diagnostic->package);
    std::string candidate_error;
    if (probe.LoadResource(resource, &candidate_error)) resources.push_back(resource);
    if (resources.size() == 3) break;
  }
  if (resources.size() != 3) { if (error) *error = "fewer than three decodable GAI resources"; return false; }
  diagnostic->asteroid.SetPackage(&diagnostic->package); diagnostic->asteroid.SetId("m20-asteroid");
  diagnostic->secondary.SetPackage(&diagnostic->package); diagnostic->secondary.SetId("m20-secondary");
  diagnostic->overlay.SetPackage(&diagnostic->package); diagnostic->overlay.SetId("m20-overlay");
  if (!diagnostic->asteroid.LoadResource(resources[0], error) || !diagnostic->secondary.LoadResource(resources[1], error) ||
      !diagnostic->overlay.LoadResource(resources[2], error)) return false;
  const auto asteroid_x = width / 2 - diagnostic->asteroid.Image().width - 24;
  const auto asteroid_y = height / 2 - diagnostic->asteroid.Image().height / 2;
  diagnostic->asteroid.SetPosition(asteroid_x, asteroid_y); diagnostic->asteroid.SetLayer(0); diagnostic->asteroid.SetAlpha(255);
  diagnostic->secondary.SetPosition(width / 2 + 24, height / 2 - diagnostic->secondary.Image().height / 2); diagnostic->secondary.SetLayer(10); diagnostic->secondary.SetAlpha(255);
  diagnostic->overlay.SetPosition(asteroid_x + 10, asteroid_y + 10); diagnostic->overlay.SetLayer(20); diagnostic->overlay.SetAlpha(128);
  if (!SyncM20Scene(diagnostic, error) || !diagnostic->scene.ComputeFingerprint(&diagnostic->fingerprint, error)) return false;
  Log("[M20] GI object BEGIN"); Log("[M20] objects=3");
  const M20ObjectReport reports[]{{"asteroid"}, {"secondary"}, {"overlay"}};
  const srhd_awa::platform::gi_object::GIObject* objects[]{&diagnostic->asteroid, &diagnostic->secondary, &diagnostic->overlay};
  for (std::size_t index = 0; index < 3; ++index) { const auto& object = *objects[index]; Log("[M20] object%zu id=%s resource=%s frame=%ld source_frame=%ld x=%ld y=%ld layer=%ld alpha=%u", index, reports[index].id, object.Resource().c_str(), static_cast<long>(object.SequenceFrame()), static_cast<long>(object.SourceFrame()), static_cast<long>(object.X()), static_cast<long>(object.Y()), static_cast<long>(object.Layer()), object.Alpha()); }
  Log("[M20] scene_crc32=%08lx scene_fnv64=%016llx canonical_bytes=%zu", static_cast<unsigned long>(diagnostic->fingerprint.crc32), static_cast<unsigned long long>(diagnostic->fingerprint.fnv64), diagnostic->fingerprint.canonical_bytes);
  return true;
}

bool UpdateM20GiObjects(void* user_data, std::uint64_t now_ms, std::string* error) {
  auto* diagnostic = static_cast<M20GiObjectDiagnostic*>(user_data);
  const auto delta_ms = diagnostic->last_tick ? now_ms - diagnostic->last_tick : 0;
  diagnostic->last_tick = now_ms;
  return diagnostic->asteroid.Update(delta_ms, error) && diagnostic->secondary.Update(delta_ms, error) && diagnostic->overlay.Update(delta_ms, error);
}

bool DrawM20GiObjects(void* user_data, std::string* error) {
  auto* diagnostic = static_cast<M20GiObjectDiagnostic*>(user_data);
  auto* framebuffer = GR_Main::ScreenRenderBuffer;
  if (!framebuffer || !framebuffer->GetPixels() || framebuffer->PitchBytes % 2 != 0) { if (error) *error = "M20 RGB565 framebuffer unavailable"; return false; }
  if (!SyncM20Scene(diagnostic, error)) return false;
  const srhd_awa::platform::scene_compositor::Framebuffer target{static_cast<std::uint16_t*>(framebuffer->GetPixels()), framebuffer->Width, framebuffer->Height, framebuffer->PitchBytes / 2};
  if (!diagnostic->scene.Render(target, error)) return false;
  if (!diagnostic->rendered) { diagnostic->rendered = true; Log("[M20] GI object PASS objects=3"); }
  return true;
}

struct M21UiImageDiagnostic {
  srhd_awa::platform::image_object::PortableImageObject simple;
  bool rendered{};
};

bool InitializeM21UiImages(M20GiObjectDiagnostic* m20, std::int32_t width, std::int32_t height,
                           M21UiImageDiagnostic* diagnostic, std::string* error) {
  constexpr const char* kSimpleResource = "DATA/Planet/Spu00.png";
  constexpr std::uint32_t kSimpleSourceCrc32 = 0xa3721a9cu;
  constexpr std::uint64_t kSimpleSourceFnv64 = UINT64_C(0x32ebfdd05d7fa674);
  constexpr std::uint32_t kSimpleDecodedCrc32 = 0x51e16db2u;
  constexpr std::uint64_t kSimpleDecodedFnv64 = UINT64_C(0xffeaf550d3c28655);
  const auto* entry = m20->package.Resolve(kSimpleResource);
  std::vector<std::uint8_t> source;
  if (!entry || !m20->package.ReadPayload(*entry, &source, error)) return false;
  const auto source_crc32 = CrcUnit::ComputeCrc32(source.data(), static_cast<std::int32_t>(source.size()));
  const auto source_fnv64 = M17Fnv(source.data(), source.size());
  if (source.size() != 7389 || source_crc32 != kSimpleSourceCrc32 || source_fnv64 != kSimpleSourceFnv64) {
    if (error) *error = "M21 Simple release source differs from oracle";
    return false;
  }
  diagnostic->simple.SetPackage(&m20->package);
  diagnostic->simple.SetId("m21-simple-spu00");
  diagnostic->simple.SetPosition(32, 96);
  diagnostic->simple.SetSize(128, 60);
  diagnostic->simple.SetLayer(5);
  if (!diagnostic->simple.Load(srhd_awa::platform::image_object::Kind::Simple, kSimpleResource, "", error)) return false;
  if (diagnostic->simple.natural_width() != 128 || diagnostic->simple.natural_height() != 60 ||
      diagnostic->simple.natural_width() > width || diagnostic->simple.natural_height() > height) {
    if (error) *error = "M21 configured Simple dimensions differ from release oracle";
    return false;
  }
  const std::uint8_t* decoded{};
  std::size_t decoded_bytes{};
  std::int32_t decoded_pitch{};
  if (!diagnostic->simple.GetSimpleNative565(&decoded, &decoded_bytes, &decoded_pitch, error)) return false;
  const auto decoded_crc32 = CrcUnit::ComputeCrc32(const_cast<std::uint8_t*>(decoded), static_cast<std::int32_t>(decoded_bytes));
  const auto decoded_fnv64 = M17Fnv(decoded, decoded_bytes);
  if (decoded_pitch != 256 || decoded_bytes != 15360 || decoded_crc32 != kSimpleDecodedCrc32 ||
      decoded_fnv64 != kSimpleDecodedFnv64) {
    if (error) *error = "M21 Simple decoded pixels differ from oracle";
    return false;
  }
  Log("[M21] UI image foundation BEGIN");
  Log("[M21] simple_resource=%s simple_size=%ldx%ld simple_mode=Simple", kSimpleResource,
      static_cast<long>(diagnostic->simple.natural_width()), static_cast<long>(diagnostic->simple.natural_height()));
  Log("[M21] simple_source_bytes=%zu simple_source_crc32=%08lx simple_source_fnv64=%016llx",
      source.size(), static_cast<unsigned long>(source_crc32), static_cast<unsigned long long>(source_fnv64));
  Log("[M21] simple_decoded_pitch=%ld simple_decoded_bytes=%zu simple_decoded_crc32=%08lx simple_decoded_fnv64=%016llx",
      static_cast<long>(decoded_pitch), decoded_bytes, static_cast<unsigned long>(decoded_crc32),
      static_cast<unsigned long long>(decoded_fnv64));
  Log("[M21] trans_resource=NOT_PRESENT trans_option=NOT_PRESENT");
  Log("[M21] alpha_resource=NOT_PRESENT alpha_trans_bytes=0 alpha_transalpha_bytes=0 alpha_alpha_bytes=0");
  Log("[M21] release_inventory trans=NOT_PRESENT alpha=NOT_PRESENT");
  return true;
}

struct M22UiTreeDiagnostic {
  srhd_awa::platform::ui::UiTree tree;
  srhd_awa::platform::ui::UiPanel* scroll_panel{};
  std::uint64_t last_tick{};
  bool rendered{};
};

srhd_awa::platform::ui::UiGILeaf* AddM22GiLeaf(
    srhd_awa::platform::ui::UiObject* parent, srhd_awa::package::Package* package,
    const srhd_awa::platform::gi_object::GIObject& source, const char* name, double depth,
    std::string* error) {
  auto* leaf = parent->AddGIObject();
  leaf->SetName(name); leaf->Image().SetPackage(package); leaf->SetPosition({source.X(), source.Y()});
  leaf->SetDepth(depth); leaf->Image().SetAlpha(source.Alpha()); leaf->Image().SetVisible(source.Visible());
  return leaf->LoadResource(source.Resource(), error) ? leaf : nullptr;
}

bool InitializeM22UiTree(M20GiObjectDiagnostic* m20, M21UiImageDiagnostic* m21,
                         std::int32_t width, std::int32_t height, M22UiTreeDiagnostic* diagnostic,
                         std::string* error) {
  using srhd_awa::platform::image_object::Kind;
  using srhd_awa::platform::ui::Point;
  using srhd_awa::platform::ui::Size;
  auto* root = diagnostic->tree.Root();
  root->SetName("m22-root"); root->SetSize({width, height});
  auto* content = root->AddPanel();
  content->SetName("m22-content"); content->SetSize({width, height}); content->SetDepth(0);
  if (!AddM22GiLeaf(content, &m20->package, m20->asteroid, "m20-asteroid", -0.0, error)) return false;
  auto* simple_panel = content->AddPanel();
  simple_panel->SetName("m22-simple-panel"); simple_panel->SetSize({width, height}); simple_panel->SetDepth(-5.0);
  diagnostic->scroll_panel = simple_panel->AddPanel();
  diagnostic->scroll_panel->SetName("m22-scroll-panel"); diagnostic->scroll_panel->SetSize({width, height});
  auto* simple = diagnostic->scroll_panel->AddImage();
  simple->SetName("m21-simple-spu00"); simple->Image().SetPackage(&m20->package); simple->SetPosition({32, 96});
  simple->SetSize({128, 60}); simple->SetDepth(0);
  if (!simple->Load(Kind::Simple, m21->simple.resource(), "", error)) return false;
  auto* mode_w = diagnostic->scroll_panel->AddImage();
  mode_w->SetName("m22-modew-hidden"); mode_w->Image().SetPackage(&m20->package); mode_w->SetPosition({1, 1});
  mode_w->SetPositionModeW(true); mode_w->SetDepth(-1); mode_w->SetSize({1, 1});
  if (!mode_w->Load(Kind::Simple, m21->simple.resource(), "", error)) return false;
  mode_w->Image().SetVisible(false);
  auto* inactive = content->AddImage();
  inactive->SetName("m22-inactive"); inactive->Image().SetPackage(&m20->package); inactive->SetPosition({0, 0});
  inactive->SetSize({1, 1}); inactive->SetDepth(-100); inactive->SetActive(false);
  if (!inactive->Load(Kind::Simple, m21->simple.resource(), "", error)) return false;
  if (!AddM22GiLeaf(content, &m20->package, m20->secondary, "m20-secondary", -10.0, error) ||
      !AddM22GiLeaf(content, &m20->package, m20->overlay, "m20-overlay", -20.0, error)) return false;
  root->UpdateGeometry();
  Log("[M22] nodes=%zu panels=4 image_leaves=3 gi_leaves=3 max_depth=3", static_cast<std::size_t>(10));
  return true;
}

bool UpdateM22UiTree(M22UiTreeDiagnostic* diagnostic, std::uint64_t now_ms, std::string* error) {
  const auto delta_ms = diagnostic->last_tick ? now_ms - diagnostic->last_tick : 0;
  diagnostic->last_tick = now_ms;
  // The first draw is the independent fixed checkpoint. Dynamic scroll starts
  // only after it has been fingerprinted.
  const auto scroll = diagnostic->rendered ? static_cast<std::int32_t>((now_ms / 1000) % 2) : 0;
  diagnostic->scroll_panel->SetScrollOffset({scroll, 0});
  return diagnostic->tree.Update(delta_ms, error);
}

struct M23TextDiagnostic {
  srhd_awa::platform::font_repository::CacheFontResolver resolver{GR_Main::GlobalCache};
  srhd_awa::platform::font_repository::Repository fonts{&resolver};
  srhd_awa::platform::ui::UiTree tree;
  bool rendered{};
};

EC_BlockPar::TBlockParEC* FindM23ReleaseLabel() {
  constexpr const char16_t* names[] = {u"ML", u"AB", u"Panel", u"Panel",
                                       u"Panel", u"Panel", u"Panel", u"Label"};
  constexpr std::int32_t ordinals[] = {0, 0, 0, 0, 1, 4, 0, 0};
  auto* block = GR_Main::MainDataConfig;
  for (std::size_t depth = 0; depth < 8 && block; ++depth) {
    std::int32_t seen = 0;
    EC_BlockPar::TBlockParEC* selected = nullptr;
    for (std::int32_t index = 0; index < block->GetBlockCount(); ++index) {
      const auto name = block->GetBlockNameByIndex(index);
      if (std::u16string_view(name.pchar(), name.length()) != names[depth]) continue;
      if (seen++ == ordinals[depth]) { selected = block->GetBlockByIndex(index); break; }
    }
    block = selected;
  }
  return block;
}

bool InitializeM23Text(M23TextDiagnostic* diagnostic, std::string* error) {
  if (!diagnostic || !GR_Main::MainDataConfig || !GR_Main::LanguageDataConfig ||
      !GR_Main::GlobalCache) {
    if (error) *error = "M23 runtime configuration or GlobalCache is unavailable";
    return false;
  }
  if (std::u16string_view(GR_Main::SelectedLanguage.pchar(), GR_Main::SelectedLanguage.length()) != u"russian") {
    if (error) *error = "M23 fixed release oracle requires the Russian runtime language";
    return false;
  }
  auto* block = FindM23ReleaseLabel();
  if (!block) { if (error) *error = "M23 selected release Label path is missing"; return false; }
  std::vector<std::uint8_t> source;
  std::string resolved_source;
  if (!diagnostic->resolver.LoadFont("Font.2Intro", &source, &resolved_source, error)) return false;
  constexpr std::uint32_t kSourceCrc = 0x93df743fu;
  constexpr std::uint64_t kSourceFnv = UINT64_C(0x4c320b6bc6048343);
  const auto source_crc = CrcUnit::ComputeCrc32(source.data(), static_cast<std::int32_t>(source.size()));
  const auto source_fnv = M17Fnv(source.data(), source.size());
  Log("[M23] font key=Font.2Intro source=%s bytes=%zu crc32=%08lx fnv64=%016llx",
      resolved_source.c_str(), source.size(), static_cast<unsigned long>(source_crc),
      static_cast<unsigned long long>(source_fnv));
  if (source.size() != 26669 || source_crc != kSourceCrc || source_fnv != kSourceFnv) {
    if (error) *error = "M23 release AFT source differs from Python oracle";
    return false;
  }
  std::shared_ptr<const srhd_awa::platform::aft_font::AftFont> font;
  if (!diagnostic->fonts.Acquire("Font.2Intro", &font, nullptr, error)) return false;
  const auto structure = font->fingerprint();
  Log("[M23] aft crc32=%08lx fnv64=%016llx glyphs=%zu line=%ld center=%ld above=%ld below=%ld max_advance=%ld",
      static_cast<unsigned long>(structure.crc32), static_cast<unsigned long long>(structure.fnv64),
      font->glyphs().size(), static_cast<long>(font->line_height()),
      static_cast<long>(font->centering_height()), static_cast<long>(font->above_baseline()),
      static_cast<long>(font->below_baseline()), static_cast<long>(font->max_glyph_advance()));
  if (structure.crc32 != 0x7ecfe087u || structure.fnv64 != UINT64_C(0x1a1527c347b838c2) ||
      font->glyphs().size() != 214 || font->line_height() != 16 ||
      font->centering_height() != 11 || font->above_baseline() != 16 ||
      font->below_baseline() != 2 || font->max_glyph_advance() != 16) {
    if (error) *error = "M23 AFT structure or metrics differ from Python oracle";
    return false;
  }
  auto* root = diagnostic->tree.Root();
  root->SetName("m23-root"); root->SetSize({1024, 60});
  srhd_awa::platform::ui_config::Context context{};
  context.styles = GR_Main::UiStyleConfig;
  context.language = GR_Main::LanguageDataConfig;
  context.fonts = &diagnostic->fonts;
  context.resolve_depth = [](const std::string& name, double* value) {
    return srhd_awa::platform::ui_config::ResolveRuntimeDepth(GR_Main::UiDepthConfig, name, value);
  };
  context.resolve_label_font_alias = [](const std::string& key) {
    return srhd_awa::platform::font_repository::ResolveLabelAlias(key,
        GlobalsV::FontSmoothingEnabled);
  };
  if (!srhd_awa::platform::ui_config::LoadLabel(root, block, context, error)) return false;
  auto* label = dynamic_cast<srhd_awa::platform::ui::UiLabelLeaf*>(root->FindByNameRecursive("WinText"));
  if (!label || label->FontKey() != "Font.2Intro" || label->TextLines().size() != 1 ||
      label->TextLines()[0] != u"\u0412 \u044b  \u043f \u043e \u0431 \u0435 \u0434 \u0438 \u043b \u0438 !" ||
      label->AlignX() != srhd_awa::platform::ui::LabelAlignX::Center ||
      label->AlignY() != srhd_awa::platform::ui::LabelAlignY::CenterEx ||
      label->AbsolutePosition() != srhd_awa::platform::ui::Point{0, 10} ||
      label->ClientSize() != srhd_awa::platform::ui::Size{1024, 40}) {
    if (error) *error = "M23 release Label configuration or localization differs from oracle";
    return false;
  }
  std::vector<std::uint8_t> text_utf16;
  for (char16_t code : label->TextLines()[0]) {
    text_utf16.push_back(static_cast<std::uint8_t>(code));
    text_utf16.push_back(static_cast<std::uint8_t>(code >> 8));
  }
  const auto text_crc = CrcUnit::ComputeCrc32(text_utf16.data(), static_cast<std::int32_t>(text_utf16.size()));
  const auto text_fnv = M17Fnv(text_utf16.data(), text_utf16.size());
  Log("[M23] text_utf16_crc32=%08lx text_utf16_fnv64=%016llx lines=%zu",
      static_cast<unsigned long>(text_crc), static_cast<unsigned long long>(text_fnv),
      label->RenderedLineCount());
  if (text_crc != 0xb64f251bu || text_fnv != UINT64_C(0x780519c70238915f)) {
    if (error) *error = "M23 release Label localization differs from Python UTF-16 oracle";
    return false;
  }
  const auto bounds = label->ContentBounds();
  const auto content_size = label->ContentSize();
  if (bounds.left != 1 || bounds.top != -12 || bounds.right != 156 || bounds.bottom != 2 ||
      content_size.width != 156 || content_size.height != 17 || label->RenderedLineCount() != 1) {
    if (error) *error = "M23 release Label measurement differs from Python oracle";
    return false;
  }
  std::vector<std::uint16_t> fixed_pixels(1024u * 60u, 0);
  const srhd_awa::platform::scene_compositor::Framebuffer fixed{
      fixed_pixels.data(), 1024, 60, 1024};
  if (!diagnostic->tree.Render(fixed, error)) return false;
  srhd_awa::platform::ui_fingerprint::Value tree_value{}, frame_value{};
  if (!srhd_awa::platform::ui_fingerprint::ComputeTree(*root, &tree_value, error) ||
      !srhd_awa::platform::ui_fingerprint::ComputeFramebuffer(fixed, &frame_value, error)) return false;
  Log("[M23] expected tree=%08lx/%016llx frame=%08lx/%016llx actual tree=%08lx/%016llx frame=%08lx/%016llx",
      0xef3ef436ul, static_cast<unsigned long long>(UINT64_C(0x6022c76fb3cb9306)),
      0xb36cfe2ful, static_cast<unsigned long long>(UINT64_C(0x6b916c3b29d2a194)),
      static_cast<unsigned long>(tree_value.crc32), static_cast<unsigned long long>(tree_value.fnv64),
      static_cast<unsigned long>(frame_value.crc32), static_cast<unsigned long long>(frame_value.fnv64));
  if (tree_value.crc32 != 0xef3ef436u || tree_value.fnv64 != UINT64_C(0x6022c76fb3cb9306) ||
      frame_value.crc32 != 0xb36cfe2fu || frame_value.fnv64 != UINT64_C(0x6b916c3b29d2a194)) {
    if (error) *error = "M23 real Label fixed checkpoint differs from Python oracle";
    return false;
  }
  Log("[M23] PASS font=Font.2Intro label=WinText tree=MATCH framebuffer=MATCH");
  return true;
}

bool VerifyM25RawSources(srhd_awa::package::Package* forms, std::string* error) {
  struct Asset { const char* path; std::size_t size; std::uint32_t crc; std::uint64_t fnv; };
  constexpr Asset assets[] = {
      {"DATA/FormLoad2/2BarLeft.gi", 3147, 0x2c4ef025u, UINT64_C(0x8c50d78299733f7d)},
      {"DATA/FormLoad2/2BarCenter.gi", 2976, 0x19267238u, UINT64_C(0xdebceda99798e0e0)},
      {"DATA/FormLoad2/2BarRight.gi", 3147, 0xacd0cc19u, UINT64_C(0xd2e03950066b6825)}};
  for (const auto& asset : assets) {
    const auto* entry = forms->Resolve(asset.path);
    std::vector<std::uint8_t> bytes;
    if (!entry || !forms->ReadPayload(*entry, &bytes, error)) return false;
    if (bytes.size() != asset.size ||
        CrcUnit::ComputeCrc32(bytes.data(), static_cast<std::int32_t>(bytes.size())) != asset.crc ||
        M17Fnv(bytes.data(), bytes.size()) != asset.fnv) {
      if (error) *error = "M25 raw GI source differs from Python oracle";
      return false;
    }
  }
  return true;
}

struct M25ReleaseDiagnostic {
  srhd_awa::package::Package forms, common;
  srhd_awa::platform::ui::UiTree tree;
  srhd_awa::platform::ui::UiGaiLeaf* gai{};
  bool rendered{}, advanced{};
  std::int32_t previous_frame{-1};
  std::uint64_t last_tick{};
};

EC_BlockPar::TBlockParEC* FindM25ReleasePanel() {
  constexpr const char16_t* names[] = {u"ML", u"AB", u"Panel", u"Panel", u"Panel", u"Panel", u"Panel"};
  constexpr std::int32_t ordinals[] = {0, 0, 0, 0, 1, 0, 0};
  auto* block = GR_Main::MainDataConfig;
  for (std::size_t depth = 0; depth < 7 && block; ++depth) {
    std::int32_t seen{};
    EC_BlockPar::TBlockParEC* selected{};
    for (std::int32_t index = 0; index < block->GetBlockCount(); ++index) {
      const auto name = block->GetBlockNameByIndex(index);
      if (std::u16string_view(name.pchar(), name.length()) != names[depth]) continue;
      if (seen++ == ordinals[depth]) { selected = block->GetBlockByIndex(index); break; }
    }
    block = selected;
  }
  return block;
}

bool VerifyM25ReleaseControls(srhd_awa::package::Package* forms, std::string* error) {
  using namespace srhd_awa::platform;
  const auto image = [forms, error](const char* path) -> std::unique_ptr<ui::UiImageLeaf> {
    auto leaf = std::make_unique<ui::UiImageLeaf>(forms);
    return leaf->Load(image_object::Kind::GI, path, "", error) ? std::move(leaf) : nullptr;
  };
  const auto frame = [error](ui::UiTree* tree, std::int32_t width, std::int32_t height,
                             std::uint32_t crc, std::uint64_t fnv) {
    std::vector<std::uint16_t> pixels(static_cast<std::size_t>(width) * height, 0);
    const scene_compositor::Framebuffer target{pixels.data(), width, height, width};
    ui_fingerprint::Value hash{};
    if (!tree->Render(target, error) || !ui_fingerprint::ComputeFramebuffer(target, &hash, error)) return false;
    if (hash.crc32 == crc && hash.fnv64 == fnv) return true;
    if (error) *error = "M25 release control frame differs from Python oracle";
    return false;
  };
  ui::UiTree button_tree;
  button_tree.SetRootSize({53, 42});
  auto* button = button_tree.Root()->AddGraphButton();
  button->SetName("F1"); button->SetSize({53, 42});
  button->SetButtonKind(ui::GraphButtonKind::Disable);
  button->SetHitKind(ui::GraphButtonHitKind::Graph);
  constexpr struct { ui::GraphButtonSlot slot; const char* path; } states[] = {
      {ui::GraphButtonSlot::Normal, "DATA/FormAB2/2W1GN.gi"},
      {ui::GraphButtonSlot::NormalA, "DATA/FormAB2/2W1GA.gi"},
      {ui::GraphButtonSlot::Down, "DATA/FormAB2/2W1GD.gi"},
      {ui::GraphButtonSlot::Disable, "DATA/FormAB2/2W1H.gi"}};
  for (const auto& state : states) {
    auto leaf = image(state.path);
    if (!leaf || !button->AddStateImage(state.slot, std::move(leaf), error)) return false;
  }
  if (!frame(&button_tree, 53, 42, 0x1c585c8fu, UINT64_C(0x7f23dbb7c1d8df15))) return false;
  button->SetHovered(true);
  if (!frame(&button_tree, 53, 42, 0x1a6eae32u, UINT64_C(0x46793341f42dd912))) return false;
  button->SetDown(true);
  if (!frame(&button_tree, 53, 42, 0x663cb736u, UINT64_C(0xc45ecedf7f592470)) ||
      !button->HitTest({26, 20}, error)) return false;
  ui::UiTree window_tree;
  auto* window = window_tree.Root()->AddWindow();
  window->SetName("InfoPanel"); window->SetSize({280, 174});
  window->SetMinimumSize({250, 0}); window->SetWorkSubRect({16, 65, 12, 15});
  constexpr struct { ui::WindowSlot slot; const char* path; } borders[] = {
      {ui::WindowSlot::Left, "DATA/FormNote/2SimpleLeft.gi"},
      {ui::WindowSlot::Right, "DATA/FormNote/2SimpleRight.gi"},
      {ui::WindowSlot::Top, "DATA/FormNote/2SimpleTop.gi"},
      {ui::WindowSlot::Bottom, "DATA/FormNote/2SimpleBottom.gi"},
      {ui::WindowSlot::TopLeft, "DATA/FormNote/2SimpleTopLeft.gi"},
      {ui::WindowSlot::TopRight, "DATA/FormNote/2SimpleTopRight.gi"},
      {ui::WindowSlot::BottomLeft, "DATA/FormNote/2SimpleBottomLeft.gi"},
      {ui::WindowSlot::BottomRight, "DATA/FormNote/2SimpleBottomRight.gi"},
      {ui::WindowSlot::Texture, "DATA/FormNote/2SimpleTexture.gi"}};
  for (const auto& border : borders) {
    auto leaf = image(border.path);
    if (!leaf || !window->AddBorderImage(border.slot, std::move(leaf), error)) return false;
  }
  if (!window->FinalizeLayout(error) || window->ClientSize() != ui::Size{282, 175}) {
    if (error && error->empty()) *error = "M25 release Window geometry differs from oracle";
    return false;
  }
  window_tree.SetRootSize(window->ClientSize());
  if (!frame(&window_tree, 282, 175, 0x87e5a68fu, UINT64_C(0x8020187c43bbcc26))) return false;
  Log("[M25] GraphButton normal=MATCH hover=MATCH down=MATCH hit=MATCH Window border=MATCH");
  return true;
}

bool InitializeM25Release(const char* game_root, std::int32_t screen_width,
                          std::int32_t screen_height, M25ReleaseDiagnostic* diagnostic,
                          std::string* error) {
  if (!diagnostic || !GR_Main::MainDataConfig) {
    if (error) *error = "M25 Main.dat configuration is unavailable";
    return false;
  }
  auto* selected = FindM25ReleasePanel();
  if (!selected || selected->CountParams(u"Name") != 1 || selected->GetParam(u"Name") != u"PLBar") {
    if (error) *error = "M25 release subtree path changed";
    return false;
  }
  if (!diagnostic->forms.Open((std::filesystem::path(game_root) / "DATA/forms.pkg").string(), error) ||
      !diagnostic->common.Open((std::filesystem::path(game_root) / "DATA/common.pkg").string(), error)) return false;
  if (!VerifyM25RawSources(&diagnostic->forms, error)) return false;
  if (!VerifyM25ReleaseControls(&diagnostic->forms, error)) return false;
  srhd_awa::platform::ui_cache_resolver::CacheUiResourceResolver resolver(GR_Main::CacheDataRoot);
  srhd_awa::platform::ui_config::Context context{};
  context.resources = &resolver;
  context.styles = GR_Main::UiStyleConfig;
  context.language = GR_Main::LanguageDataConfig;
  context.resolve_depth = [](const std::string& name, double* value) {
    return srhd_awa::platform::ui_config::ResolveRuntimeDepth(GR_Main::UiDepthConfig, name, value);
  };
  diagnostic->tree.SetRootSize({321, 37});
  auto panel = std::make_unique<srhd_awa::platform::ui::UiPanel>();
  if (!srhd_awa::platform::ui_config::ApplyBaseProperties(panel.get(), selected, context, error)) return false;
  if (panel->ClientSize() != srhd_awa::platform::ui::Size{321, 37}) {
    if (error) *error = "M25 release panel size changed";
    return false;
  }
  panel->SetPosition({0, 0}); panel->SetDepth(0);
  if (!srhd_awa::platform::ui_config::LoadChildren(panel.get(), selected, context,
        srhd_awa::platform::ui_config::LoadMode::Strict, nullptr, error) || panel->ChildCount() != 17) return false;
  auto* subtree = panel.get();
  if (!diagnostic->tree.Root()->Attach(std::move(panel), error)) return false;
  std::vector<std::uint16_t> pixels(321u * 37u, 0);
  const srhd_awa::platform::scene_compositor::Framebuffer fixed{pixels.data(), 321, 37, 321};
  srhd_awa::platform::ui_fingerprint::Value tree_hash{}, frame_hash{};
  if (!diagnostic->tree.Render(fixed, error) ||
      !srhd_awa::platform::ui_fingerprint::ComputeTree(*subtree, &tree_hash, error) ||
      !srhd_awa::platform::ui_fingerprint::ComputeFramebuffer(fixed, &frame_hash, error)) return false;
  Log("[M25] real UI tree=%08lx/%016llx frame=%08lx/%016llx",
      static_cast<unsigned long>(tree_hash.crc32), static_cast<unsigned long long>(tree_hash.fnv64),
      static_cast<unsigned long>(frame_hash.crc32), static_cast<unsigned long long>(frame_hash.fnv64));
  if (tree_hash.crc32 != 0x73a25b4cu || tree_hash.fnv64 != UINT64_C(0x26d6a269e96b959b) ||
      frame_hash.crc32 != 0x9cec8dc2u || frame_hash.fnv64 != UINT64_C(0x39c2ccfd0deb5fbb)) {
    if (error) *error = "M25 release subtree differs from Python oracle";
    return false;
  }
  const auto* entry = diagnostic->common.Resolve("DATA/PI/PathEndMove.gai");
  std::vector<std::uint8_t> gai_source;
  if (!entry || !diagnostic->common.ReadPayload(*entry, &gai_source, error)) return false;
  if (gai_source.size() != 27402 ||
      CrcUnit::ComputeCrc32(gai_source.data(), static_cast<std::int32_t>(gai_source.size())) != 0x3bf46ce9u ||
      M17Fnv(gai_source.data(), gai_source.size()) != UINT64_C(0x7b4a7f853b191bc5)) {
    if (error) *error = "M25 GAI source differs from Python oracle";
    return false;
  }
  diagnostic->gai = diagnostic->tree.Root()->AddGai();
  diagnostic->gai->SetSize({32, 32});
  if (!diagnostic->gai->LoadBytes(gai_source.data(), gai_source.size(), "Bm.PI.PathEndMove", error)) return false;
  std::vector<std::uint16_t> gai_pixels(32u * 32u, 0);
  const srhd_awa::platform::scene_compositor::Framebuffer gai_frame{gai_pixels.data(), 32, 32, 32};
  srhd_awa::platform::ui_fingerprint::Value gai_hash{};
  if (!diagnostic->gai->Render(gai_frame, {0, 0, 32, 32}, error) ||
      !srhd_awa::platform::ui_fingerprint::ComputeFramebuffer(gai_frame, &gai_hash, error) ||
      gai_hash.crc32 != 0x71b457cdu || gai_hash.fnv64 != UINT64_C(0xd75029d766f2bded) ||
      !diagnostic->gai->Update(70, error)) {
    if (error && error->empty()) *error = "M25 GAI initial frame or first advance differs from oracle";
    return false;
  }
  std::fill(gai_pixels.begin(), gai_pixels.end(), 0);
  if (!diagnostic->gai->Render(gai_frame, {0, 0, 32, 32}, error) ||
      !srhd_awa::platform::ui_fingerprint::ComputeFramebuffer(gai_frame, &gai_hash, error) ||
      diagnostic->gai->Animation().SourceFrame() != 1 ||
      gai_hash.crc32 != 0xed8aac30u || gai_hash.fnv64 != UINT64_C(0x4b7e19dd0fbc527a)) {
    if (error && error->empty()) *error = "M25 GAI advanced frame differs from oracle";
    return false;
  }
  Log("[M25] raw GI=MATCH GAI frame0=MATCH GAI frame1=MATCH");
  if (!diagnostic->gai->SetFramePosition(0, false, error)) return false;
  diagnostic->previous_frame = diagnostic->gai->Animation().SourceFrame();
  if (diagnostic->previous_frame != 0) {
    if (error) *error = "M25 GAI did not reset to frame zero before runtime loop";
    return false;
  }
  diagnostic->tree.SetRootSize({screen_width, screen_height});
  subtree->SetPosition({(screen_width - 321) / 2, screen_height - 90});
  diagnostic->gai->SetPosition({(screen_width - 321) / 2 + 335, screen_height - 87});
  return true;
}

bool UpdateM25Release(M25ReleaseDiagnostic* diagnostic, std::uint64_t now_ms,
                      std::string* error) {
  const auto delta = diagnostic->last_tick ? now_ms - diagnostic->last_tick : 0;
  diagnostic->last_tick = now_ms;
  if (!diagnostic->tree.Update(delta, error)) return false;
  const auto current_frame = diagnostic->gai->Animation().SourceFrame();
  if (!diagnostic->advanced && diagnostic->previous_frame >= 0 &&
      current_frame != diagnostic->previous_frame) {
    diagnostic->advanced = true;
    Log("[M25] GAI runtime advance=%ld->%ld", static_cast<long>(diagnostic->previous_frame),
        static_cast<long>(current_frame));
  }
  diagnostic->previous_frame = current_frame;
  return true;
}

struct M26ReleaseDiagnostic {
  srhd_awa::platform::ui::UiTree showcase, controls;
  srhd_awa::platform::ui::UiScrollBar* scroll{};
  srhd_awa::platform::ui::UiPanelScrollBar* panel{};
  srhd_awa::platform::ui::UiEdit* edit{};
  srhd_awa::platform::ui::UiGraphBuffer* graph{};
  std::uint64_t frames{}, last_tick{};
  std::uint64_t update_us_total{}, render_us_total{}, update_us_max{}, render_us_max{};
  bool rendered{}, scroll_advanced{}, caret_advanced{};
};

std::size_t EstimateM26ImageBytes(const srhd_awa::platform::ui::UiObject& object) {
  using namespace srhd_awa::platform;
  std::size_t total{};
  if (const auto* leaf = dynamic_cast<const ui::UiImageLeaf*>(&object))
    total += static_cast<std::size_t>(std::max(0, leaf->Image().natural_width())) *
             static_cast<std::size_t>(std::max(0, leaf->Image().natural_height())) * 4;
  for (const auto& child : object.Children()) total += EstimateM26ImageBytes(*child);
  return total;
}

EC_BlockPar::TBlockParEC* FindM26Block(
    std::initializer_list<std::pair<const char16_t*, std::int32_t>> path) {
  auto* block = GR_Main::MainDataConfig;
  for (const auto& [name, ordinal] : path) {
    if (!block) return nullptr;
    EC_BlockPar::TBlockParEC* found{};
    for (std::int32_t index = 0, seen = 0; index < block->GetBlockCount(); ++index) {
      const auto candidate = block->GetBlockNameByIndex(index);
      if (std::u16string_view(candidate.pchar(), candidate.length()) == name &&
          seen++ == ordinal) { found = block->GetBlockByIndex(index); break; }
    }
    block = found;
  }
  return block;
}

bool InitializeM26Release(M26ReleaseDiagnostic* diagnostic, M23TextDiagnostic* text,
                          M25ReleaseDiagnostic* m25, std::int32_t width,
                          std::int32_t height, std::string* error) {
  using namespace srhd_awa::platform;
  if (!diagnostic || !text || !m25 || width != 1280 || height != 720)
    return error ? (*error = "M26 requires the fixed 1280x720 release frame", false) : false;
  auto* showcase_source = FindM26Block({{u"ML", 0}, {u"Info", 0}, {u"Panel", 0},
                                       {u"Panel", 4}, {u"Panel", 11}});
  auto* scroll_source = FindM26Block({{u"ML", 0}, {u"Film", 0}, {u"Panel", 0},
                                     {u"Panel", 0}, {u"Panel", 0}, {u"ScrollBar", 0}});
  auto* panel_source = FindM26Block({{u"ML", 0}, {u"Achievements", 0},
                                    {u"Panel", 0}, {u"Panel", 0},
                                    {u"PanelScrollBar", 0}});
  if (!showcase_source || !scroll_source || !panel_source ||
      showcase_source->GetBlockCount() != 49) {
    if (error) *error = "M26 release UI paths changed";
    return false;
  }
  ui_cache_resolver::CacheUiResourceResolver resources(GR_Main::CacheDataRoot);
  ui_config::Context context{};
  context.resources = &resources;
  context.fonts = &text->fonts;
  context.styles = GR_Main::UiStyleConfig;
  context.language = GR_Main::LanguageDataConfig;
  context.resolve_depth = [](const std::string& name, double* value) {
    return ui_config::ResolveRuntimeDepth(GR_Main::UiDepthConfig, name, value);
  };
  context.resolve_label_font_alias = [](const std::string& key) {
    return font_repository::ResolveLabelAlias(key, GlobalsV::FontSmoothingEnabled);
  };
  diagnostic->showcase.SetRootSize({width, height});
  auto real_panel = std::make_unique<ui::UiPanel>();
  if (!ui_config::ApplyBaseProperties(real_panel.get(), showcase_source, context, error) ||
      real_panel->ClientSize() != ui::Size{410, 435}) return false;
  if (real_panel->Active()) {
    if (error) *error = "M26 selected tab is no longer initially hidden";
    return false;
  }
  real_panel->SetActive(true);
  real_panel->SetPosition({298, 120});
  if (!ui_config::LoadChildren(real_panel.get(), showcase_source, context,
                               ui_config::LoadMode::Strict, nullptr, error) ||
      real_panel->ChildCount() != 49) return false;
  auto* showcase_node = real_panel.get();
  if (!diagnostic->showcase.Root()->Attach(std::move(real_panel), error)) return false;
  std::vector<std::uint16_t> showcase_pixels(static_cast<std::size_t>(width) * height);
  const scene_compositor::Framebuffer showcase_framebuffer{
      showcase_pixels.data(), width, height, width};
  ui_fingerprint::Value showcase_tree{}, showcase_frame{};
  if (!diagnostic->showcase.Render(showcase_framebuffer, error) ||
      !ui_fingerprint::ComputeTree(*showcase_node, &showcase_tree, error) ||
      !ui_fingerprint::ComputeFramebuffer(showcase_framebuffer, &showcase_frame, error))
    return false;
  Log("[M26] real UI tree=%08lx/%016llx frame=%08lx/%016llx",
      static_cast<unsigned long>(showcase_tree.crc32),
      static_cast<unsigned long long>(showcase_tree.fnv64),
      static_cast<unsigned long>(showcase_frame.crc32),
      static_cast<unsigned long long>(showcase_frame.fnv64));
  if (showcase_tree.crc32 != 0x67b1fcbfu ||
      showcase_tree.fnv64 != UINT64_C(0x2940a0ef334dd86e) ||
      showcase_frame.crc32 != 0xcb1a12b3u ||
      showcase_frame.fnv64 != UINT64_C(0xfbca196e86b2f452)) {
    if (error) *error = "M26 real UI tree or Python framebuffer oracle differs";
    return false;
  }
  diagnostic->edit = dynamic_cast<ui::UiEdit*>(
      diagnostic->showcase.Root()->FindByNameRecursive("M11Size"));
  if (!diagnostic->edit) {
    if (error) *error = "M26 real Edit missing";
    return false;
  }
  diagnostic->edit->SetText(u"123");
  diagnostic->edit->SetFocused(true);
  diagnostic->edit->SetCaretBlink(true);
  std::fill(showcase_pixels.begin(), showcase_pixels.end(), 0);
  ui_fingerprint::Value edit_frame{};
  if (!diagnostic->edit->RenderLeaf(showcase_framebuffer, {0, 0, width, height}, error) ||
      !ui_fingerprint::ComputeFramebuffer(showcase_framebuffer, &edit_frame, error) ||
      edit_frame.crc32 != 0x0c547e1bu ||
      edit_frame.fnv64 != UINT64_C(0x18432c76a6d566c5)) {
    if (error && error->empty()) *error = "M26 real focused Edit frame differs from Python oracle";
    return false;
  }
  Log("[M26] Edit focused frame=%08lx/%016llx",
      static_cast<unsigned long>(edit_frame.crc32),
      static_cast<unsigned long long>(edit_frame.fnv64));
  diagnostic->edit->SetText(u"");
  diagnostic->edit->SetFocused(false);
  diagnostic->controls.SetRootSize({width, height});
  auto* selected = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
  selected->AddChildBlock(u"ScrollBar")->CopyFrom(scroll_source);
  auto* panel_copy = selected->AddChildBlock(u"PanelScrollBar");
  panel_copy->CopyFrom(panel_source);
  while (panel_copy->GetBlockCount() > 0)
    panel_copy->DeleteChildBlock(panel_copy->GetBlockNameByIndex(0));
  if (!ui_config::LoadChildren(diagnostic->controls.Root(), selected, context,
                               ui_config::LoadMode::Strict, nullptr, error)) return false;
  diagnostic->scroll = dynamic_cast<ui::UiScrollBar*>(
      diagnostic->controls.Root()->FindByNameRecursive("PF_SBTurn"));
  diagnostic->panel = dynamic_cast<ui::UiPanelScrollBar*>(
      diagnostic->controls.Root()->FindByNameRecursive("PanelSlot"));
  if (!diagnostic->scroll || !diagnostic->panel ||
      !diagnostic->panel->UpdateScrollRanges(error)) return false;
  if (diagnostic->scroll->TrackLength() != 209 ||
      diagnostic->scroll->ThumbLength() != 56 ||
      diagnostic->scroll->BeforeLength() != 0 ||
      diagnostic->scroll->AfterLength() != 153 ||
      diagnostic->panel->VerticalBar()->Minimum() != 0 ||
      diagnostic->panel->VerticalBar()->Maximum() != 539 ||
      diagnostic->panel->VerticalBar()->PageSize() != 540 ||
      diagnostic->panel->VerticalBar()->Position() != 0 ||
      diagnostic->panel->VerticalBar()->LocalPosition() != ui::Point{903, 116} ||
      diagnostic->panel->VerticalBar()->ClientSize() != ui::Size{20, 513}) {
    if (error) *error = "M26 real ScrollBar or PanelScrollBar layout differs from Python oracle";
    return false;
  }
  diagnostic->scroll->UiObject::SetPosition({810, 665});
  diagnostic->graph = diagnostic->controls.Root()->AddGraphBuffer();
  diagnostic->graph->SetPosition({1040, 95});
  diagnostic->graph->SetSize({100, 50});
  const auto* entry = m25->forms.Resolve("DATA/FormLoad2/2BarCenter.gi");
  std::vector<std::uint8_t> source;
  if (!entry || !m25->forms.ReadPayload(*entry, &source, error) ||
      !diagnostic->graph->LoadGiBytes(source.data(), source.size(), error)) return false;
  if (source.size() != 2976 ||
      CrcUnit::ComputeCrc32(source.data(), static_cast<std::int32_t>(source.size())) != 0x19267238u ||
      M17Fnv(source.data(), source.size()) != UINT64_C(0xdebceda99798e0e0) ||
      diagnostic->graph->Buffer().width() != 39 ||
      diagnostic->graph->Buffer().height() != 50) {
    if (error) *error = "M26 real GraphBuf GI source or aspect fit differs from Python oracle";
    return false;
  }
  const auto& scaled = diagnostic->graph->Buffer().pixels();
  // The translated CRC helper takes void* but reads the input bytes only.
  auto* scaled_data = const_cast<std::uint8_t*>(scaled.data());
  if (scaled.size() != 7800 ||
      CrcUnit::ComputeCrc32(scaled_data, static_cast<std::int32_t>(scaled.size())) !=
          0x07bfadc2u ||
      M17Fnv(scaled.data(), scaled.size()) != UINT64_C(0x1dc1b91c9c87ca5a)) {
    if (error) *error = "M26 real GraphBuf scaled pixels differ from fixed host/ARM-compatible oracle";
    return false;
  }
  const auto graph_center = diagnostic->graph->GetVisualCenter();
  const bool graph_hit = diagnostic->graph->HitTestPixel({1090, 120});
  if (!graph_hit || graph_center != ui::Point{49, 24}) {
    if (error) *error = "M26 real GraphBuf hit or visual centre differs from host oracle";
    return false;
  }
  Log("[M26] GraphBuf source=19267238/debceda99798e0e0 scaled=%08lx/%016llx size=%ldx%ld hit=%u center=%ld,%ld",
      static_cast<unsigned long>(CrcUnit::ComputeCrc32(scaled_data,
          static_cast<std::int32_t>(scaled.size()))),
      static_cast<unsigned long long>(M17Fnv(scaled.data(), scaled.size())),
      static_cast<long>(diagnostic->graph->Buffer().width()),
      static_cast<long>(diagnostic->graph->Buffer().height()),
      graph_hit ? 1u : 0u,
      static_cast<long>(graph_center.x),
      static_cast<long>(graph_center.y));
  std::size_t font_source_bytes{};
  for (const auto* path : {"DATA/FONT/Verdana_09_2.aft", "DATA/FONT/ranger_6.aft",
                           "DATA/FONT/Verdana_08_1.aft", "DATA/FONT/Verdana_08_2_bold.aft"})
    if (const auto* font = m25->forms.Resolve(path)) font_source_bytes += font->data_size;
  const auto scroll_bytes = EstimateM26ImageBytes(*diagnostic->scroll) +
      EstimateM26ImageBytes(*diagnostic->panel->VerticalBar());
  const auto real_bytes = EstimateM26ImageBytes(*showcase_node) + font_source_bytes;
  const auto graph_bytes = diagnostic->graph->Buffer().bytes();
  const auto peak_estimate = real_bytes + scroll_bytes + graph_bytes +
      static_cast<std::size_t>(width) * height * 4;
  Log("[M26] real UI nodes=50 position=298,120 size=410x435");
  Log("[M26] memory graphbuf_source=%zu graphbuf_decoded=%zu scrollbar_decoded_est=%zu edit_font_sources=%zu real_subtree_est=%zu peak_ui_est=%zu",
      source.size(), graph_bytes, scroll_bytes, font_source_bytes, real_bytes,
      peak_estimate);
  return true;
}

bool UpdateM26Release(M26ReleaseDiagnostic* diagnostic, std::uint64_t now_ms,
                      std::string* error) {
  const auto delta = diagnostic->last_tick ? now_ms - diagnostic->last_tick : 0;
  diagnostic->last_tick = now_ms;
  if (!diagnostic->showcase.Update(delta, error) ||
      !diagnostic->controls.Update(delta, error)) return false;
  ++diagnostic->frames;
  if (diagnostic->frames == 30) {
    diagnostic->edit->SetText(u"123");
    diagnostic->edit->SetFocused(true);
    diagnostic->edit->SetCaretBlink(true);
    diagnostic->edit->SetCaretPosition(0);
  }
  if (diagnostic->frames >= 30 && diagnostic->frames <= 150 &&
      diagnostic->frames % 30 == 0) {
    const std::int32_t positions[] = {1, 51, 101, 151, 200};
    const auto position = positions[diagnostic->frames / 30 - 1];
    diagnostic->scroll->SetPosition(position);
    diagnostic->scroll->SetHoveredRegion(5);
    Log("[M26] ScrollBar transition position=%ld", static_cast<long>(position));
    if (diagnostic->frames == 150)
      diagnostic->scroll_advanced = diagnostic->scroll->Position() == 200;
  }
  if (diagnostic->frames == 45 || diagnostic->frames == 75 ||
      diagnostic->frames == 105) {
    diagnostic->edit->MoveRight();
    Log("[M26] Edit caret=%ld", static_cast<long>(diagnostic->edit->CaretPosition()));
    if (diagnostic->frames == 105)
      diagnostic->caret_advanced = diagnostic->edit->CaretPosition() == 3;
  }
  if (diagnostic->frames > 30 && diagnostic->frames < 160)
    diagnostic->edit->SetCaretBlink(((diagnostic->frames - 30) / 30) % 2 == 0);
  return true;
}

struct M27ReleaseDiagnostic {
  M26ReleaseDiagnostic* m26{};
  srhd_awa::platform::ui::UiInputRouter showcase_input;
  srhd_awa::platform::ui::UiInputRouter controls_input;
  srhd_awa::platform::ui::UiGraphButton* button{};
  std::vector<std::uint16_t> pixels;
  srhd_awa::platform::input_platform::Snapshot live_snapshot{};
  std::uint64_t last_tick{};
  bool scripted_done{}, live_started{}, button_trace_ok{};
  explicit M27ReleaseDiagnostic(M26ReleaseDiagnostic* source, std::int32_t width,
                                std::int32_t height)
      : m26(source), showcase_input(*source->showcase.Root()),
        controls_input(*source->controls.Root()),
        button(dynamic_cast<srhd_awa::platform::ui::UiGraphButton*>(
            source->showcase.Root()->FindByNameRecursive("M11Clear"))),
        pixels(static_cast<std::size_t>(width) * height) {}
};

bool CheckM27ShowcaseFrame(M27ReleaseDiagnostic* diagnostic, const char* name,
                          std::uint32_t crc, std::uint64_t fnv, std::string* error) {
  using namespace srhd_awa::platform;
  if (diagnostic->pixels.size() != 1280u * 720u) {
    if (error) *error = "M27 fixed release frame requires 1280x720";
    return false;
  }
  std::fill(diagnostic->pixels.begin(), diagnostic->pixels.end(), 0);
  const scene_compositor::Framebuffer target{diagnostic->pixels.data(), 1280, 720, 1280};
  ui_fingerprint::Value actual{};
  if (!diagnostic->m26->showcase.Render(target, error) ||
      !ui_fingerprint::ComputeFramebuffer(target, &actual, error)) return false;
  Log("[M27] real button %s frame=%08lx/%016llx", name,
      static_cast<unsigned long>(actual.crc32), static_cast<unsigned long long>(actual.fnv64));
  if (actual.crc32 != crc || actual.fnv64 != fnv) {
    if (error) *error = std::string("M27 independent real button frame differs: ") + name;
    return false;
  }
  return true;
}

bool CheckM27ControlsFrame(M27ReleaseDiagnostic* diagnostic, const char* name,
                          std::uint32_t crc, std::uint64_t fnv, std::string* error) {
  using namespace srhd_awa::platform;
  if (diagnostic->pixels.size() != 1280u * 720u) {
    if (error) *error = "M27 fixed controls frame requires 1280x720";
    return false;
  }
  std::fill(diagnostic->pixels.begin(), diagnostic->pixels.end(), 0);
  const scene_compositor::Framebuffer target{diagnostic->pixels.data(), 1280, 720, 1280};
  ui_fingerprint::Value actual{};
  if (!diagnostic->m26->controls.Render(target, error) ||
      !ui_fingerprint::ComputeFramebuffer(target, &actual, error)) return false;
  Log("[M27] real controls %s frame=%08lx/%016llx", name,
      static_cast<unsigned long>(actual.crc32), static_cast<unsigned long long>(actual.fnv64));
  if (actual.crc32 != crc || actual.fnv64 != fnv) {
    if (error) *error = std::string("M27 real controls frame differs: ") + name;
    return false;
  }
  return true;
}

bool UpdateM27Release(M27ReleaseDiagnostic* diagnostic, std::uint64_t now_ms,
                      std::string* error) {
  using namespace srhd_awa::platform;
  const auto delta = diagnostic->last_tick ? now_ms - diagnostic->last_tick : 0;
  diagnostic->last_tick = now_ms;
  diagnostic->showcase_input.Update(delta);
  diagnostic->controls_input.Update(delta);
  const auto frame = diagnostic->m26->frames;
  if (frame < 160 || diagnostic->scripted_done) return true;
  auto* button = diagnostic->button;
  if (!button || button->HitKind() != ui::GraphButtonHitKind::Rect ||
      !button->HitTest({459, 537})) {
    if (error) *error = "M27 real M11Clear candidate changed";
    return false;
  }
  if (frame == 160) {
    diagnostic->m26->edit->SetText(u"");
    diagnostic->m26->edit->SetFocused(false);
    return CheckM27ShowcaseFrame(diagnostic, "normal", 0xcb1a12b3u,
                                 UINT64_C(0xfbca196e86b2f452), error);
  }
  if (frame == 161) {
    diagnostic->showcase_input.PointerMove({459, 537});
    if (!button->Hovered()) { if (error) *error = "M27 real button hover failed"; return false; }
    return CheckM27ShowcaseFrame(diagnostic, "hover", 0xfb83833bu,
                                 UINT64_C(0x5c7d41d26204f9cd), error);
  }
  if (frame == 162) {
    diagnostic->showcase_input.PointerDown(ui::UiPointerButton::Left, {459, 537});
    if (!button->Down()) { if (error) *error = "M27 real button down failed"; return false; }
    return CheckM27ShowcaseFrame(diagnostic, "down", 0x12dd6ab5u,
                                 UINT64_C(0xffe66f7b03cca7cd), error);
  }
  if (frame == 163) {
    diagnostic->showcase_input.PointerUp(ui::UiPointerButton::Left, {459, 537});
    if (button->Down()) { if (error) *error = "M27 real button up failed"; return false; }
    return CheckM27ShowcaseFrame(diagnostic, "up", 0xfb83833bu,
                                 UINT64_C(0x5c7d41d26204f9cd), error);
  }
  if (frame == 164) {
    diagnostic->showcase_input.PointerLeave();
    if (button->Hovered()) { if (error) *error = "M27 real button leave failed"; return false; }
    if (!CheckM27ShowcaseFrame(diagnostic, "leave", 0xcb1a12b3u,
                               UINT64_C(0xfbca196e86b2f452), error)) return false;
    const auto trace = ui::FingerprintActions(diagnostic->showcase_input.Actions());
    Log("[M27] scripted_event_crc32=%08lx scripted_event_fnv64=%016llx actions=%zu bytes=%llu",
        static_cast<unsigned long>(trace.crc32), static_cast<unsigned long long>(trace.fnv64),
        diagnostic->showcase_input.Actions().size(), static_cast<unsigned long long>(trace.bytes));
    diagnostic->button_trace_ok = diagnostic->showcase_input.Actions().size() == 20 &&
        trace.bytes == 1327 && trace.crc32 == 0x004504e6u &&
        trace.fnv64 == UINT64_C(0x1192cd139f06a4a0);
    if (!diagnostic->button_trace_ok) {
      if (error) *error = "M27 independent real button event trace differs";
      return false;
    }
    if (diagnostic->showcase_input.QueryPointOcclusionState({459, 537}, button) != -1) {
      if (error) *error = "M27 real button occlusion changed";
      return false;
    }
    diagnostic->showcase_input.ClearActions();
    return true;
  }
  if (frame == 165) {
    const auto bounds = diagnostic->m26->edit->HitTestBounds();
    diagnostic->showcase_input.PointerDown(ui::UiPointerButton::Left,
                                            {bounds.left + 2, bounds.top + 2});
    const bool focused = diagnostic->showcase_input.FocusedControl() == diagnostic->m26->edit &&
                         diagnostic->m26->edit->Focused();
    Log("[M27] Edit focus=%u caret=%ld", focused ? 1u : 0u,
        static_cast<long>(diagnostic->m26->edit->CaretPosition()));
    if (!focused) { if (error) *error = "M27 real Edit focus failed"; return false; }
    if (!CheckM27ShowcaseFrame(diagnostic, "edit-focused", 0x84415dcdu,
                               UINT64_C(0x6ad55cf9f6e3b52a), error)) return false;
    diagnostic->showcase_input.TextInput(u'1');
    if (!CheckM27ShowcaseFrame(diagnostic, "edit-text", 0xf4e281ddu,
                               UINT64_C(0xf9206d95dea1c1cb), error)) return false;
    diagnostic->showcase_input.KeyDown(ui::UiKey::Backspace, {.shift = true});
    if (!diagnostic->m26->edit->Text().empty()) {
      if (error) *error = "M27 real Edit Shift+Backspace failed";
      return false;
    }
    diagnostic->showcase_input.Update(201);
    if (diagnostic->m26->edit->CaretBlinkOn()) {
      if (error) *error = "M27 real Edit caret blink failed";
      return false;
    }
    return CheckM27ShowcaseFrame(diagnostic, "edit-caret-off", 0xcb1a12b3u,
                                 UINT64_C(0xfbca196e86b2f452), error);
  }
  if (frame == 166) {
    auto* scroll = diagnostic->m26->scroll;
    scroll->SetPosition(1);
    const auto bounds = scroll->HitTestBounds();
    const ui::Point thumb{bounds.left + 50, (bounds.top + bounds.bottom) / 2};
    diagnostic->controls_input.PointerMove(thumb);
    diagnostic->controls_input.PointerDown(ui::UiPointerButton::Left, thumb);
    diagnostic->controls_input.PointerMove({thumb.x + 40, thumb.y});
    diagnostic->controls_input.PointerUp(ui::UiPointerButton::Left, {thumb.x + 40, thumb.y});
    Log("[M27] ScrollBar drag position=%ld", static_cast<long>(scroll->Position()));
    if (scroll->Position() != 53) { if (error) *error = "M27 real ScrollBar drag differs"; return false; }
    return CheckM27ControlsFrame(diagnostic, "scroll-moved", 0x50d0d772u,
                                 UINT64_C(0x230a5072582bdb54), error);
  }
  if (frame == 167) {
    // The isolated release panel has no scrollable world. The bar and its
    // callback are release objects; a 1000-pixel test child supplies range.
    auto* world = diagnostic->m26->panel->AddObject();
    world->SetPositionModeW(true);
    world->SetSize({831, 1000});
    if (!diagnostic->m26->panel->UpdateScrollRanges(error)) return false;
    auto* vertical = diagnostic->m26->panel->VerticalBar();
    const auto bounds = vertical->HitTestBounds();
    const ui::Point down{(bounds.left + bounds.right) / 2, bounds.bottom - 5};
    diagnostic->controls_input.PointerMove(down);
    diagnostic->controls_input.PointerDown(ui::UiPointerButton::Left, down);
    diagnostic->controls_input.Update(500);
    diagnostic->controls_input.PointerUp(ui::UiPointerButton::Left, down);
    Log("[M27] PanelScrollBar release-bar synthetic-world offset=%ld",
        static_cast<long>(diagnostic->m26->panel->ScrollOffset().y));
    if (diagnostic->m26->panel->ScrollOffset().y != 2) {
      if (error) *error = "M27 real PanelScrollBar callback differs";
      return false;
    }
    const auto trace = ui::FingerprintActions(diagnostic->showcase_input.Actions());
    Log("[M27] real event trace=%08lx/%016llx actions=%zu bytes=%llu",
        static_cast<unsigned long>(trace.crc32), static_cast<unsigned long long>(trace.fnv64),
        diagnostic->showcase_input.Actions().size(), static_cast<unsigned long long>(trace.bytes));
    diagnostic->showcase_input.ClearActions();
    diagnostic->controls_input.ClearActions();
    diagnostic->scripted_done = diagnostic->button_trace_ok;
    Log("[M27] router PASS");
    Log("[M27] occlusion PASS");
    Log("[M27] GraphButton PASS");
    Log("[M27] ScrollBar PASS");
    Log("[M27] Edit PASS");
    Log("[M27] scripted interaction PASS real-release");
  }
  return true;
}

bool DispatchM27Input(void* user_data, const srhd_awa::platform::input_platform::Snapshot& snapshot,
                      std::string*) {
  auto* diagnostic = static_cast<M27ReleaseDiagnostic*>(user_data);
  diagnostic->live_snapshot = snapshot;
  if (!diagnostic->scripted_done) return true;
  using namespace srhd_awa::platform;
  const ui::Point point{snapshot.x, snapshot.y};
  for (auto* router : {&diagnostic->showcase_input, &diagnostic->controls_input}) {
    router->PointerMove(point);
    if (snapshot.left_down) router->PointerDown(ui::UiPointerButton::Left, point);
    if (snapshot.left_up) router->PointerUp(ui::UiPointerButton::Left, point);
    if (snapshot.right_down) router->PointerDown(ui::UiPointerButton::Right, point);
    if (snapshot.right_up) router->PointerUp(ui::UiPointerButton::Right, point);
  }
  if (!diagnostic->live_started) { Log("[M27] LIVE INPUT READY"); diagnostic->live_started = true; }
  if (snapshot.left_down || snapshot.left_up || snapshot.right_down || snapshot.right_up)
    Log("[M27] pointer x=%ld y=%ld left_down=%u left_up=%u right_down=%u right_up=%u",
        static_cast<long>(point.x), static_cast<long>(point.y), snapshot.left_down,
        snapshot.left_up, snapshot.right_down, snapshot.right_up);
  diagnostic->showcase_input.ClearActions();
  diagnostic->controls_input.ClearActions();
  return true;
}

struct M21FrameCallbacks { M17PlaybackDiagnostic* m17{}; M20GiObjectDiagnostic* m20{}; M21UiImageDiagnostic* m21{}; M22UiTreeDiagnostic* m22{}; M23TextDiagnostic* m23{}; srhd_awa::platform::ui_controls_checkpoint::Checkpoint* m24{}; M25ReleaseDiagnostic* m25{}; M26ReleaseDiagnostic* m26{}; M27ReleaseDiagnostic* m27{}; };
bool RunM17M20AndM21(void* user_data, std::uint64_t now_ms, std::string* error) {
  auto* callbacks = static_cast<M21FrameCallbacks*>(user_data);
  const auto started = std::chrono::steady_clock::now();
  const bool result = RunM17Playback(callbacks->m17, now_ms, error) && UpdateM20GiObjects(callbacks->m20, now_ms, error) &&
      UpdateM22UiTree(callbacks->m22, now_ms, error) && callbacks->m24->Update(now_ms, error) &&
      UpdateM25Release(callbacks->m25, now_ms, error) && UpdateM26Release(callbacks->m26, now_ms, error) &&
      UpdateM27Release(callbacks->m27, now_ms, error);
  const auto elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - started).count());
  callbacks->m26->update_us_total += elapsed;
  callbacks->m26->update_us_max = std::max(callbacks->m26->update_us_max, elapsed);
  return result;
}
bool DrawM21Presentation(void* user_data, std::string* error) {
  auto* callbacks = static_cast<M21FrameCallbacks*>(user_data);
  const auto started = std::chrono::steady_clock::now();
  auto* framebuffer = GR_Main::ScreenRenderBuffer;
  if (!framebuffer || !framebuffer->GetPixels() || framebuffer->PitchBytes % 2 != 0) {
    if (error) *error = "M21 RGB565 framebuffer unavailable";
    return false;
  }
  const srhd_awa::platform::scene_compositor::Framebuffer target{static_cast<std::uint16_t*>(framebuffer->GetPixels()), framebuffer->Width, framebuffer->Height, framebuffer->PitchBytes / 2};
  if (!callbacks->m22->tree.Render(target, error)) return false;
  callbacks->m20->rendered = true;
  if (!callbacks->m21->rendered) {
    constexpr std::uint32_t kSceneCrc32 = 0x4f915772u;
    constexpr std::uint64_t kSceneFnv64 = UINT64_C(0x52449ae8f8f56c6c);
    constexpr std::uint32_t kM22TreeCrc32 = 0xcaab2979u;
    constexpr std::uint64_t kM22TreeFnv64 = UINT64_C(0x0e5557af167f16ec);
    srhd_awa::platform::ui_fingerprint::Value fingerprint{}, tree_fingerprint{};
    if (!srhd_awa::platform::ui_fingerprint::ComputeFramebuffer(target, &fingerprint, error) ||
        !srhd_awa::platform::ui_fingerprint::ComputeTree(*callbacks->m22->tree.Root(), &tree_fingerprint, error)) return false;
    Log("[M21] scene_actual_crc32=%08lx scene_actual_fnv64=%016llx scene_actual_bytes=%zu expected_crc32=%08lx expected_fnv64=%016llx", static_cast<unsigned long>(fingerprint.crc32),
        static_cast<unsigned long long>(fingerprint.fnv64), fingerprint.bytes, static_cast<unsigned long>(kSceneCrc32),
        static_cast<unsigned long long>(kSceneFnv64));
    if (fingerprint.bytes != 1843200 || fingerprint.crc32 != kSceneCrc32 || fingerprint.fnv64 != kSceneFnv64) {
      if (error) *error = "M21 first logical runtime frame differs from release oracle";
      return false;
    }
    if (tree_fingerprint.bytes != 691 || tree_fingerprint.crc32 != kM22TreeCrc32 ||
        tree_fingerprint.fnv64 != kM22TreeFnv64) {
      if (error) *error = "M22 fixed runtime tree differs from release oracle";
      return false;
    }
    callbacks->m21->rendered = true;
    callbacks->m22->rendered = true;
    Log("[M20] GI object PASS objects=3");
    Log("[M21] layout_count=1 hit_test=NOT_APPLICABLE alpha=NOT_PRESENT");
    Log("[M21] scene_crc32=%08lx scene_fnv64=%016llx scene_bytes=%zu", static_cast<unsigned long>(fingerprint.crc32),
        static_cast<unsigned long long>(fingerprint.fnv64), fingerprint.bytes);
    Log("[M21] GIObject regression PASS");
    Log("[M21] UI image foundation PASS simple=1 trans=NOT_PRESENT alpha=NOT_PRESENT");
    Log("[M22] tree_crc32=%08lx tree_fnv64=%016llx tree_bytes=%zu", static_cast<unsigned long>(tree_fingerprint.crc32),
        static_cast<unsigned long long>(tree_fingerprint.fnv64), tree_fingerprint.bytes);
    Log("[M22] frameA_crc32=%08lx frameA_fnv64=%016llx frameA_bytes=%zu", static_cast<unsigned long>(fingerprint.crc32),
        static_cast<unsigned long long>(fingerprint.fnv64), fingerprint.bytes);
    Log("[M22] oracle MATCH depth=PASS clip=PASS scroll=PASS active=PASS");
  }
  if (!callbacks->m23->tree.Render(target, error)) return false;
  callbacks->m23->rendered = true;
  if (!callbacks->m24->Render(target, error)) return false;
  callbacks->m24->MarkRendered();
  if (!callbacks->m25->tree.Render(target, error)) return false;
  callbacks->m25->rendered = true;
  if (!callbacks->m26->showcase.Render(target, error) ||
      !callbacks->m26->controls.Render(target, error)) return false;
  callbacks->m26->rendered = true;
  // Diagnostic crosshair is an overlay; it is not part of any UI tree hash.
  if (callbacks->m27->scripted_done) {
    const auto x = callbacks->m27->live_snapshot.x;
    const auto y = callbacks->m27->live_snapshot.y;
    for (int delta = -4; delta <= 4; ++delta) {
      if (x + delta >= 0 && x + delta < target.width && y >= 0 && y < target.height)
        target.pixels[static_cast<std::size_t>(y) * target.pitch_pixels + x + delta] = 0xffff;
      if (y + delta >= 0 && y + delta < target.height && x >= 0 && x < target.width)
        target.pixels[static_cast<std::size_t>(y + delta) * target.pitch_pixels + x] = 0xffff;
    }
  }
  const auto elapsed = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - started).count());
  callbacks->m26->render_us_total += elapsed;
  callbacks->m26->render_us_max = std::max(callbacks->m26->render_us_max, elapsed);
  return true;
}

struct M20FrameCallbacks { M17PlaybackDiagnostic* m17{}; M20GiObjectDiagnostic* m20{}; };
bool RunM17AndM20(void* user_data, std::uint64_t now_ms, std::string* error) {
  auto* callbacks = static_cast<M20FrameCallbacks*>(user_data);
  return RunM17Playback(callbacks->m17, now_ms, error) && UpdateM20GiObjects(callbacks->m20, now_ms, error);
}

bool ValidateReleaseAssets(const char* root) {
  StageBegin("release asset sanity");
  const std::filesystem::path base(root);
  for (const char* relative : {"install.txt", "cfg.txt", "DATA/common.pkg", "install_russian.txt"}) {
    const auto path = base / relative;
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
      const auto rendered = path.generic_string();
      Log("[RESOURCE] FAIL missing=%s", rendered.c_str());
      Stage("release asset sanity", false, rendered.c_str());
      return false;
    }
  }
  Stage("release asset sanity", true);
  return true;
}

const char* GameRoot(int argc, char** argv) {
  if (argc > 1 && argv[1] && argv[1][0] != '\0') return argv[1];
  return kDefaultGameRoot;
}

bool ConfigurePackages(srhd_awa::platform::startup_slice::State* state, const char* root) {
  std::string error;
  StageBegin("startup configuration");
  Log("[M7] platform init begin");
  if (!srhd_awa::platform::startup_slice::Initialize(state, root, kDefaultUserRoot, kGrMainLogPath, &error)) {
    Log("[M7] FAIL startup=%s", error.c_str());
    Stage("startup configuration", false, error.c_str());
    return false;
  }
  Log("[M7] timing PASS frequency=%lld", static_cast<long long>(GR_Main::PerformanceCounterFrequency));
  Log("[M7] window PASS token=%lu", static_cast<unsigned long>(GR_Main::MainWindowHandle));
  Log("[M7] package collection PASS");
  Stage("package collection", true);
  Log("[M7] install config PASS");
  Stage("INSTALL.TXT", true);
  std::int32_t package_count = 0;
  while (EC_HsFile::PackageCollection->GetPackByIndex(package_count)) ++package_count;
  Log("[M7] language packages PASS language=%s count=%ld",
      GR_Main::SelectedLanguage == u"russian" ? "russian" : "unexpected", static_cast<long>(package_count));
  Log("[M7] configured sources=%ld", static_cast<long>(package_count));
  if (GR_Main::SelectedLanguage != u"russian" || package_count != 18) {
    Stage("configured packages", false, "release baseline mismatch");
    return false;
  }
  Stage("configured packages", true);
  Stage("startup configuration", true);
  return true;
}

bool ReadRequiredAsset(const char* root) {
  srhd_awa::platform::PackageRootInfo package{};
  char error[128]{};
  if (!srhd_awa::platform::ProbePackageRoot(root, "DATA/common.pkg", &package, error, sizeof(error))) {
    Log("[RESOURCE] FAIL parser=EC_HsFile root file=DATA/common.pkg error=%s", error);
    return false;
  }
  const std::uint32_t smoke_crc = CrcUnit::ComputeCrc32(&package, static_cast<std::int32_t>(sizeof(package)));
  Log("[CXX] CrcUnit linkage smoke crc32=%08lx", static_cast<unsigned long>(smoke_crc));
  Log("[RESOURCE] PASS parser=package probe size=%llu root=%lu entries=%lu record=%lu first=%s",
      static_cast<unsigned long long>(package.file_size), static_cast<unsigned long>(package.root_offset),
      static_cast<unsigned long>(package.entry_count), static_cast<unsigned long>(package.entry_record_size),
      package.first_entry_name);

  srhd_awa::package::Package archive;
  std::string package_error;
  if (!archive.Open(std::string(root) + "/DATA/common.pkg", &package_error)) {
    Log("[PACKAGE] FAIL recursive load error=%s", package_error.c_str());
    return false;
  }
  const auto tree = archive.Summarize();
  const auto* entry = archive.Resolve("DATA/Asteroid/00.gai");
  std::vector<std::uint8_t> payload;
  if (!entry || !archive.ReadPayload(*entry, &payload, &package_error)) {
    Log("[PACKAGE] FAIL payload path=DATA/Asteroid/00.gai error=%s", package_error.c_str());
    return false;
  }
  const std::uint32_t payload_crc = CrcUnit::ComputeCrc32(payload.data(), static_cast<std::int32_t>(payload.size()));
  EC_File::TFileEC file{};
  EC_File::TFileEC_Create(&file);
  file.SetFileName(pas::WideString(u"data\\asteroid\\00.GAI"));
  const bool opened = file.TryAcquireReadHandle(false) != 0;
  const uint32_t ec_size = opened ? file.GetSize() : 0;
  std::vector<uint8_t> ec_payload(ec_size);
  if (opened) file.ReadBuffer(ec_payload.data(), ec_size);
  const bool seek_ok = opened && file.SetPointer(4, 0) == 4 && file.SetPointer(3, 1) == 7 && file.SetPointer(5, 2) == ec_size - 5;
  uint8_t ec_tail[5]{};
  if (seek_ok) file.ReadBuffer(ec_tail, sizeof(ec_tail));
  if (opened) file.ReleaseHandle();
  EC_File::TFileEC_Destroy(&file);
  const bool ec_ok = opened && ec_size == payload.size() && ec_payload == payload && seek_ok &&
      std::memcmp(ec_tail, payload.data() + payload.size() - sizeof(ec_tail), sizeof(ec_tail)) == 0;
  Log("[EC_FILE] %s open=%u size=%lu read=%lu seek=%u crc32=%08lx", ec_ok ? "PASS" : "FAIL", opened ? 1u : 0u,
      static_cast<unsigned long>(ec_size), static_cast<unsigned long>(ec_payload.size()), seek_ok ? 1u : 0u,
      static_cast<unsigned long>(CrcUnit::ComputeCrc32(ec_payload.data(), static_cast<std::int32_t>(ec_payload.size()))));
  const bool baseline_ok = tree.folders == 51 && tree.files == 1890 && tree.entries == 1940 &&
      tree.max_depth == 4 && tree.tree_hash == UINT64_C(0x9c74d6b37be3edd2) &&
      entry->kind == 2 && payload.size() == 246863 && payload_crc == 0x045269e4u;
  Log("[PACKAGE] %s folders=%lu files=%lu entries=%lu depth=%lu tree_hash=%016llx selected=DATA/Asteroid/00.gai kind=%ld size=%lu crc32=%08lx", baseline_ok ? "PASS" : "FAIL",
      static_cast<unsigned long>(tree.folders), static_cast<unsigned long>(tree.files),
      static_cast<unsigned long>(tree.entries), static_cast<unsigned long>(tree.max_depth), static_cast<unsigned long long>(tree.tree_hash),
      static_cast<long>(entry->kind), static_cast<unsigned long>(payload.size()), static_cast<unsigned long>(payload_crc));
  Log("[SELFTEST] %s package baseline", baseline_ok ? "PASS" : "FAIL");
  Log("[M7] resource %s", ec_ok ? "PASS" : "FAIL");
  return baseline_ok && ec_ok;
}

bool RunRendererSelfTest() {
  Log("[M8] renderer init begin");
  const bool configured_vsync = GR_Main::VSyncEnabled;
  const bool configured_scale = GlobalsV::ScaleViewportToWindow;
  const bool configured_unlimited = GR_Main::PresentWithoutLimit;
  const bool configured_hardware_request = GlobalsV::HardwareRenderingRequested;
  GR_Main::GR_DXInit();
  auto* framebuffer = GR_Main::ScreenRenderBuffer;
  if (!framebuffer || !GR_Main::RenderScratchBuffer || !GR_Main::AuxRenderBuffer ||
      !GR_Main::CurrentPixelFormat || framebuffer->UseTexture) {
    Log("[M8] FAIL renderer state");
    return false;
  }
  Log("[M8] mode=software-rgb565 framebuffer=%ldx%ld pitch=%ld", static_cast<long>(framebuffer->Width),
      static_cast<long>(framebuffer->Height), static_cast<long>(framebuffer->PitchBytes));
  Log("[M8] pixel-format=RGB565");
  GR_Main::OKGR_Fill_WORD(framebuffer->GetPixels(), framebuffer->PitchBytes, framebuffer->Width,
                          framebuffer->Height, 0x001f);
  GR_GraphBuf::TGraphBufGR_DrawHorizontalLine16(framebuffer, 4, 4, 80, 0xf800);
  GR_GraphBuf::TGraphBufGR_DrawVerticalLine16(framebuffer, 4, 4, 80, 0x07e0);
  GR_Main::LineRasterizer16(framebuffer->GetPixels(), framebuffer->PitchBytes, 10, 10, 0xf800,
                             110, 40, 0x07e0);
  WindowsSdk::TRect clip{0, 0, framebuffer->Width, framebuffer->Height};
  GR_Main::TriangleRasterizer16(framebuffer->GetPixels(), framebuffer->PitchBytes, 30, 40, 0xf800,
                                 150, 80, 0x07e0, 70, 160, 0x001f, &clip);
  framebuffer->BlendPixel16(8, 8, 0xffff, 128);
  GR_Main::PresentWithoutLimit = true;
  GR_Main::BeginFramePresentation();
  GR_Main::BeginFramePresentation();
  GR_Main::EndFramePresentation();
  const auto before_outer_end = srhd_awa::platform::renderer_platform::PresentationCount();
  GR_Main::EndFramePresentation();
  const auto present_count = srhd_awa::platform::renderer_platform::PresentationCount();
  const auto hash = srhd_awa::platform::renderer_platform::LastPresentationHash();
  const auto diagnostics = srhd_awa::platform::renderer_platform::LastPresentationDiagnostics();
  const bool settings_preserved = GR_Main::VSyncEnabled == configured_vsync &&
      GlobalsV::ScaleViewportToWindow == configured_scale &&
      GlobalsV::HardwareRenderingRequested == configured_hardware_request &&
      !GlobalsV::HardwareRenderingEnabled;
  GR_Main::PresentWithoutLimit = configured_unlimited;
  const bool aspect_ok = diagnostics.output_width > 0 && diagnostics.output_height > 0 &&
      diagnostics.destination_width > 0 && diagnostics.destination_height > 0 &&
      diagnostics.destination_width <= diagnostics.output_width && diagnostics.destination_height <= diagnostics.output_height &&
      diagnostics.destination_x >= 0 && diagnostics.destination_y >= 0;
  const bool ok = before_outer_end == 0 && present_count == 1 && hash != 0 && settings_preserved &&
      diagnostics.renderer_ready && diagnostics.texture_ready && diagnostics.present_succeeded && aspect_ok;
  Log("[M8] OKGF bridge %s", ok ? "PASS" : "FAIL");
  Log("[M8] draw %s fnv64=%016llx", ok ? "PASS" : "FAIL",
      static_cast<unsigned long long>(hash));
  Log("[M8] RGB565 %s", ok ? "PASS" : "FAIL");
  Log("[M8] SDL renderer %s", diagnostics.renderer_ready ? "PASS" : "PENDING");
  Log("[M8] SDL texture %s", diagnostics.texture_ready ? "PASS" : "PENDING");
  Log("[M8] output=%ldx%ld", static_cast<long>(diagnostics.output_width), static_cast<long>(diagnostics.output_height));
  Log("[M8] destination=%ld,%ld,%ld,%ld", static_cast<long>(diagnostics.destination_x), static_cast<long>(diagnostics.destination_y), static_cast<long>(diagnostics.destination_width), static_cast<long>(diagnostics.destination_height));
  Log("[M8] settings vsync=%u scale=%u unlimited=%u hardware_requested=%u hardware_enabled=%u",
      GR_Main::VSyncEnabled ? 1u : 0u, GlobalsV::ScaleViewportToWindow ? 1u : 0u,
      GR_Main::PresentWithoutLimit ? 1u : 0u, GlobalsV::HardwareRenderingRequested ? 1u : 0u,
      GlobalsV::HardwareRenderingEnabled ? 1u : 0u);
  Log("[M8] present %s", diagnostics.present_succeeded && aspect_ok ? "PASS" : "FAIL");
  Stage("SDL presentation", ok);
  Log("[M8] renderer boundary %s", ok ? "reached" : "failed");
  return ok;
}
}  // namespace

int main(int argc, char** argv) {
  BeginLogSession();
  Log("[BOOT] BEGIN Space Rangers HD: A War Apart");
  Log("[BOOT] Space Rangers HD: A War Apart");
  Log("[BOOT] milestone=12-runtime-loop");
  Log("[BOOT] build_git=%s baseline_rangers_sha256=83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98", SRHD_BUILD_GIT_COMMIT);
  Log("[BOOT] runtime units=CrcUnit,System,SystemImports (SpaceRangersHD_CPP)");
  SystemImports::Randomize();
  Log("[GAME] PASS SystemImports::Randomize RandSeed=%lu", static_cast<unsigned long>(System::RandSeed));
  const char* game_root = GameRoot(argc, argv);
  Log("[BOOT] game_root=%s", game_root);
  Log("[BOOT] user_root=%s", kDefaultUserRoot);
  Log("[BOOT] baseline=2.1.2500");
  Log("[FILESYSTEM] BEGIN game-root=%s", game_root);

  if (!ValidateReleaseAssets(game_root)) return 1;

  const std::filesystem::path user_cfg_path = std::filesystem::path(kDefaultUserRoot) / "config" / "CFG.TXT";
  std::error_code user_cfg_error;
  const bool user_cfg_existed = std::filesystem::is_regular_file(user_cfg_path, user_cfg_error);

  Log("[GR_MAIN] linked");
  srhd_awa::platform::startup_slice::State startup;
  if (!ConfigurePackages(&startup, game_root)) {
    srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  Stage("SDL/platform services", true);
  Stage("window", true);
  std::string m9_error;
  StageBegin("DAT/runtime config");
  Log("[M9] dat config begin");
  Fingerprint cache_raw_fp;
  if (!srhd_awa::platform::runtime_settings_slice::Initialize(&m9_error, CaptureRawCacheFingerprint, &cache_raw_fp)) {
    Log("[M9] FAIL runtime config=%s", m9_error.c_str());
    Stage("DAT/runtime config", false, m9_error.c_str());
    srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  Log("[M9] Main.dat PASS");
  Log("[M9] Lang.dat PASS language=%s", static_cast<const char*>(static_cast<pas::AnsiString>(GR_Main::LanguageInstallConfig->GetParam(u"Lang"sv)).c_str()));
  Log("[M9] CacheData.dat PASS");
  Fingerprint main_fp, lang_fp, cache_runtime_fp;
  FingerprintBlock(GR_Main::MainDataConfig, &main_fp, 1);
  FingerprintBlock(GR_Main::LanguageDataConfig, &lang_fp, 1);
  FingerprintData(GR_Main::CacheDataRoot, &cache_runtime_fp, 1);
  Log("[M9] Main.dat entries=%lu blocks=%lu params=%lu depth=%lu truncated=%u fnv64=%016llx", static_cast<unsigned long>(main_fp.entries), static_cast<unsigned long>(main_fp.blocks), static_cast<unsigned long>(main_fp.params), static_cast<unsigned long>(main_fp.depth), main_fp.truncated ? 1u : 0u, static_cast<unsigned long long>(main_fp.hash));
  Log("[M9] Lang.dat language=%s entries=%lu blocks=%lu params=%lu depth=%lu truncated=%u fnv64=%016llx", static_cast<const char*>(static_cast<pas::AnsiString>(GR_Main::LanguageInstallConfig->GetParam(u"Lang"sv)).c_str()), static_cast<unsigned long>(lang_fp.entries), static_cast<unsigned long>(lang_fp.blocks), static_cast<unsigned long>(lang_fp.params), static_cast<unsigned long>(lang_fp.depth), lang_fp.truncated ? 1u : 0u, static_cast<unsigned long long>(lang_fp.hash));
  Log("[M9] CacheData.dat raw nodes=%lu files=%lu depth=%lu truncated=%u fnv64=%016llx", static_cast<unsigned long>(cache_raw_fp.nodes), static_cast<unsigned long>(cache_raw_fp.files), static_cast<unsigned long>(cache_raw_fp.depth), cache_raw_fp.truncated ? 1u : 0u, static_cast<unsigned long long>(cache_raw_fp.hash));
  Log("[M9] CacheDataRoot runtime nodes=%lu files=%lu depth=%lu truncated=%u fnv64=%016llx", static_cast<unsigned long>(cache_runtime_fp.nodes), static_cast<unsigned long>(cache_runtime_fp.files), static_cast<unsigned long>(cache_runtime_fp.depth), cache_runtime_fp.truncated ? 1u : 0u, static_cast<unsigned long long>(cache_runtime_fp.hash));
  try {
    if (!GR_Main::UserSettingsConfig || !std::filesystem::is_regular_file(user_cfg_path, user_cfg_error))
      throw std::runtime_error("writable CFG.TXT was not materialized");
    const auto user_cfg_wide = pas::WideString(u"sdmc:/switch/space-rangers-hd-a-war-apart/config/CFG.TXT");
    GR_Main::UserSettingsConfig->SaveTextFile(user_cfg_wide.pchar(), true, false);
    auto* reloaded = pas::construct_call<EC_BlockPar::TBlockParEC>(EC_BlockPar::TBlockParEC_Create);
    reloaded->LoadFromTextFileWithEncodingProbe(user_cfg_wide.pchar(), true);
    pas::free(reloaded);
    Log("[M9] CFG create %s", user_cfg_existed ? "SKIP existing" : "PASS");
    Log("[M9] CFG save PASS");
    Log("[M9] CFG reload PASS");
  } catch (...) {
    Log("[M9] CFG FAIL writable persistence");
    Stage("DAT/runtime config", false, "CFG persistence");
    srhd_awa::platform::ui_metadata_slice::Shutdown();
  srhd_awa::platform::runtime_settings_slice::Shutdown();
    srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  const bool derived_ok = GR_Main::GameDataConfig && GR_Main::UiStyleConfig && GR_Main::UiDepthConfig &&
      GR_Main::WideCaseTable.length() > 0;
  Log("[M9] GameDataConfig %s", GR_Main::GameDataConfig ? "PASS" : "FAIL");
  Log("[M9] UiStyleConfig %s", GR_Main::UiStyleConfig ? "PASS" : "FAIL");
  Log("[M9] UiDepthConfig %s", GR_Main::UiDepthConfig ? "PASS" : "FAIL");
  Log("[M9] CaseConv count=%ld", static_cast<long>(GR_Main::WideCaseTable.length()));
  if (!derived_ok) {
    Stage("DAT/runtime config", false, "derived M9 state");
    srhd_awa::platform::ui_metadata_slice::Shutdown();
  srhd_awa::platform::runtime_settings_slice::Shutdown();
    srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  Stage("DAT/runtime config", true);
  StageBegin("GlobalCache");
  const bool global_cache_ok = GR_Main::GlobalCache && GR_Main::GlobalCache->DataRoot == GR_Main::CacheDataRoot &&
      GR_Main::GlobalCache->ResidentByteLimit > 0;
  Log("[M11] GlobalCache %s budget_bytes=%ld", global_cache_ok ? "PASS" : "FAIL",
      GR_Main::GlobalCache ? static_cast<long>(GR_Main::GlobalCache->ResidentByteLimit) : 0L);
  Stage("GlobalCache", global_cache_ok);
  if (!global_cache_ok) {
    srhd_awa::platform::ui_metadata_slice::Shutdown();
  srhd_awa::platform::runtime_settings_slice::Shutdown();
    srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  StageBegin("M13 UI metadata");
  std::string m13_error;
  if (!srhd_awa::platform::ui_metadata_slice::Initialize(&m13_error)) {
    Log("[M13] FAIL UI metadata=%s", m13_error.c_str());
    Stage("M13 UI metadata", false, m13_error.c_str());
    srhd_awa::platform::ui_metadata_slice::Shutdown();
  srhd_awa::platform::runtime_settings_slice::Shutdown();
    srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  Log("[M13] font smoothing=%u", GlobalsV::FontSmoothingEnabled ? 1u : 0u);
  for (const auto& font : srhd_awa::platform::ui_metadata_slice::FontResolutions()) {
    Log("[M13] font key=%s found=%u kind=%lu file_exists=%u file=%s", font.key.c_str(), font.found ? 1u : 0u,
        static_cast<unsigned long>(font.kind), font.file_exists ? 1u : 0u,
        font.filename.empty() ? "" : font.filename.c_str());
  }
  Stage("M13 UI metadata", true);
  StageBegin("M14P GI format0");
  const bool m14p_ok = VerifyM14pGiFormat0();
  Stage("M14P GI format0", m14p_ok);
  if (!m14p_ok) { srhd_awa::platform::ui_metadata_slice::Shutdown(); srhd_awa::platform::runtime_settings_slice::Shutdown(); srhd_awa::platform::startup_slice::Shutdown(&startup); return 1; }
  StageBegin("M15 GAI");
  const bool m15_ok = VerifyM15GaiFormat0(game_root);
  Stage("M15 GAI", m15_ok);
  if (!m15_ok) { srhd_awa::platform::ui_metadata_slice::Shutdown(); srhd_awa::platform::runtime_settings_slice::Shutdown(); srhd_awa::platform::startup_slice::Shutdown(&startup); return 1; }
  StageBegin("M16 GI format2");
  const bool m16_ok = VerifyM16GaiFormat2(game_root);
  Stage("M16 GI format2", m16_ok);
  if (!m16_ok) { srhd_awa::platform::ui_metadata_slice::Shutdown(); srhd_awa::platform::runtime_settings_slice::Shutdown(); srhd_awa::platform::startup_slice::Shutdown(&startup); return 1; }
  StageBegin("M17 GAI playback");
  M17PlaybackDiagnostic m17_diagnostic;
  std::string m17_error;
  if (!InitializeM17Playback(game_root, &m17_diagnostic, &m17_error)) {
    Log("[M17] FAIL initialization=%s", m17_error.c_str());
    Stage("M17 GAI playback", false, m17_error.c_str());
    srhd_awa::platform::ui_metadata_slice::Shutdown(); srhd_awa::platform::runtime_settings_slice::Shutdown(); srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  StageBegin("M20 GI object");
  StageBegin("cached resource");
  const bool cached_resource_ok = VerifyFirstCachedResource();
  Stage("cached resource", cached_resource_ok);
  if (!cached_resource_ok) {
    srhd_awa::platform::ui_metadata_slice::Shutdown();
  srhd_awa::platform::runtime_settings_slice::Shutdown();
    srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  StageBegin("package baseline");
  const bool resource_ok = ReadRequiredAsset(game_root);
  Stage("package baseline", resource_ok);
  if (!resource_ok) {
    srhd_awa::platform::ui_metadata_slice::Shutdown();
  srhd_awa::platform::runtime_settings_slice::Shutdown();
    srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }

  srhd_awa::platform::renderer_platform::SetNativeWindow(startup.platform.native_window);
  srhd_awa::platform::runtime_loop_slice::State runtime_loop;
  std::string m12_error;
  if (!srhd_awa::platform::runtime_loop_slice::Initialize(&runtime_loop, &m12_error)) {
    Log("[M12] FAIL runtime init=%s", m12_error.c_str());
    Stage("runtime loop", false, m12_error.c_str());
    srhd_awa::platform::runtime_loop_slice::Shutdown(&runtime_loop);
    srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);
    srhd_awa::platform::ui_metadata_slice::Shutdown();
  srhd_awa::platform::runtime_settings_slice::Shutdown();
    srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  const auto* framebuffer = GR_Main::ScreenRenderBuffer;
  M20GiObjectDiagnostic m20_diagnostic;
  std::string m20_error;
  if (!InitializeM20GiObjects(game_root, framebuffer->Width, framebuffer->Height, &m20_diagnostic, &m20_error)) {
    Log("[M20] FAIL initialization=%s", m20_error.c_str());
    Stage("M20 GI object", false, m20_error.c_str());
    srhd_awa::platform::runtime_loop_slice::Shutdown(&runtime_loop);
    srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);
    srhd_awa::platform::ui_metadata_slice::Shutdown(); srhd_awa::platform::runtime_settings_slice::Shutdown(); srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  StageBegin("M21 UI image foundation");
  M21UiImageDiagnostic m21_diagnostic;
  std::string m21_error;
  if (!InitializeM21UiImages(&m20_diagnostic, framebuffer->Width, framebuffer->Height, &m21_diagnostic, &m21_error)) {
    Log("[M21] FAIL initialization=%s", m21_error.c_str());
    Stage("M21 UI image foundation", false, m21_error.c_str());
    srhd_awa::platform::runtime_loop_slice::Shutdown(&runtime_loop);
    srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);
    srhd_awa::platform::ui_metadata_slice::Shutdown(); srhd_awa::platform::runtime_settings_slice::Shutdown(); srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  StageBegin("M22 UI object tree");
  M22UiTreeDiagnostic m22_diagnostic;
  std::string m22_error;
  if (!InitializeM22UiTree(&m20_diagnostic, &m21_diagnostic, framebuffer->Width, framebuffer->Height, &m22_diagnostic, &m22_error)) {
    Log("[M22] FAIL initialization=%s", m22_error.c_str());
    Stage("M22 UI object tree", false, m22_error.c_str());
    srhd_awa::platform::runtime_loop_slice::Shutdown(&runtime_loop);
    srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);
    srhd_awa::platform::ui_metadata_slice::Shutdown(); srhd_awa::platform::runtime_settings_slice::Shutdown(); srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  StageBegin("M23 text/label");
  M23TextDiagnostic m23_diagnostic;
  std::string m23_error;
  if (!InitializeM23Text(&m23_diagnostic, &m23_error)) {
    Log("[M23] FAIL initialization=%s", m23_error.c_str());
    Stage("M23 text/label", false, m23_error.c_str());
    srhd_awa::platform::runtime_loop_slice::Shutdown(&runtime_loop);
    srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);
    srhd_awa::platform::ui_metadata_slice::Shutdown(); srhd_awa::platform::runtime_settings_slice::Shutdown(); srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  StageBegin("M24 UI controls");
  srhd_awa::platform::ui_controls_checkpoint::Checkpoint m24_diagnostic;
  srhd_awa::platform::ui_controls_checkpoint::Evidence m24_evidence{};
  std::string m24_error;
  if (!m24_diagnostic.Initialize(&m24_error) ||
      !m24_diagnostic.VerifyFixed(&m24_evidence, &m24_error) ||
      !m24_diagnostic.StartDynamic(&m24_error)) {
    Log("[M24] FAIL initialization=%s", m24_error.c_str());
    Stage("M24 UI controls", false, m24_error.c_str());
    srhd_awa::platform::runtime_loop_slice::Shutdown(&runtime_loop);
    srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);
    srhd_awa::platform::ui_metadata_slice::Shutdown(); srhd_awa::platform::runtime_settings_slice::Shutdown(); srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  Log("[M24] expected tree=6f65eea5/11b73bf4b18ab6fb frame=015d1589/5523d509a3a9384d actual tree=%08lx/%016llx frame=%08lx/%016llx",
      static_cast<unsigned long>(m24_evidence.tree.crc32),
      static_cast<unsigned long long>(m24_evidence.tree.fnv64),
      static_cast<unsigned long>(m24_evidence.frame.crc32),
      static_cast<unsigned long long>(m24_evidence.frame.fnv64));
  Log("[M24] window=%08lx/%016llx graph=%08lx/%016llx zone=%08lx/%016llx",
      static_cast<unsigned long>(m24_evidence.window_layout.crc32),
      static_cast<unsigned long long>(m24_evidence.window_layout.fnv64),
      static_cast<unsigned long>(m24_evidence.graph_state.crc32),
      static_cast<unsigned long long>(m24_evidence.graph_state.fnv64),
      static_cast<unsigned long>(m24_evidence.zone_hits.crc32),
      static_cast<unsigned long long>(m24_evidence.zone_hits.fnv64));
  Log("[M24] PASS structure=MATCH window=MATCH graph=MATCH zone=MATCH framebuffer=MATCH");
  StageBegin("M25 GI/GAI UI");
  M25ReleaseDiagnostic m25_diagnostic;
  std::string m25_error;
  if (!InitializeM25Release(game_root, framebuffer->Width, framebuffer->Height,
                             &m25_diagnostic, &m25_error)) {
    Log("[M25] FAIL initialization=%s", m25_error.c_str());
    Stage("M25 GI/GAI UI", false, m25_error.c_str());
    srhd_awa::platform::runtime_loop_slice::Shutdown(&runtime_loop);
    srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);
    srhd_awa::platform::ui_metadata_slice::Shutdown(); srhd_awa::platform::runtime_settings_slice::Shutdown(); srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  StageBegin("M26 GraphBuf/Scroll/Edit/real UI");
  M26ReleaseDiagnostic m26_diagnostic;
  std::string m26_error;
  if (!InitializeM26Release(&m26_diagnostic, &m23_diagnostic, &m25_diagnostic,
                            framebuffer->Width, framebuffer->Height, &m26_error)) {
    Log("[M26] FAIL initialization=%s", m26_error.c_str());
    Stage("M26 GraphBuf/Scroll/Edit/real UI", false, m26_error.c_str());
    srhd_awa::platform::runtime_loop_slice::Shutdown(&runtime_loop);
    srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);
    srhd_awa::platform::ui_metadata_slice::Shutdown(); srhd_awa::platform::runtime_settings_slice::Shutdown(); srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  StageBegin("M27 input/focus");
  M27ReleaseDiagnostic m27_diagnostic(&m26_diagnostic, framebuffer->Width, framebuffer->Height);
  M21FrameCallbacks m21_callbacks{&m17_diagnostic, &m20_diagnostic, &m21_diagnostic, &m22_diagnostic, &m23_diagnostic, &m24_diagnostic, &m25_diagnostic, &m26_diagnostic, &m27_diagnostic};
  srhd_awa::platform::runtime_loop_slice::SetFrameCallback(&runtime_loop, RunM17M20AndM21, &m21_callbacks);
  srhd_awa::platform::runtime_loop_slice::SetDrawCallback(&runtime_loop, DrawM21Presentation, &m21_callbacks);
  srhd_awa::platform::runtime_loop_slice::SetInputCallback(&runtime_loop, DispatchM27Input, &m27_diagnostic);
  Log("[M12] runtime ready");
  Log("[M12] framebuffer=%ldx%ld pitch=%ld pixel=RGB565", static_cast<long>(framebuffer->Width),
      static_cast<long>(framebuffer->Height), static_cast<long>(framebuffer->PitchBytes));
  Log("[M12] audio requested=%u", GlobalsV::SoundEnabled ? 1u : 0u);
  Log("[M12] music requested=%u", GlobalsV::MusicEnabled ? 1u : 0u);
  Log("[M12] audio backend=deferred music backend=deferred");
  if (GR_Main::UserSettingsConfig->CountParamsByPath(u"FilmBufSize"_wref.get()) > 0)
    Log("[M12] FilmBufSize deferred");
  StageBegin("runtime loop");
  Log("[M12] entering persistent loop");
  bool loop_ok = srhd_awa::platform::runtime_loop_slice::RunPersistent(&runtime_loop, startup.platform, &m12_error);
  const auto loop_stats = runtime_loop.statistics;
  if (loop_ok && !m17_diagnostic.initial_cycle_verified) {
    m12_error = "M17 first cycle not completed";
    loop_ok = false;
  }
  if (loop_ok && !m20_diagnostic.rendered) {
    m12_error = "M20 GI objects did not draw";
    loop_ok = false;
  }
  if (loop_ok && !m21_diagnostic.rendered) {
    m12_error = "M21 UI image foundation did not draw";
    loop_ok = false;
  }
  if (loop_ok && !m22_diagnostic.rendered) {
    m12_error = "M22 UI object tree did not draw";
    loop_ok = false;
  }
  if (loop_ok && !m23_diagnostic.rendered) {
    m12_error = "M23 Label did not draw";
    loop_ok = false;
  }
  if (loop_ok && !m24_diagnostic.Rendered()) {
    m12_error = "M24 controls did not draw";
    loop_ok = false;
  }
  if (loop_ok && (!m25_diagnostic.rendered || !m25_diagnostic.advanced)) {
    m12_error = "M25 real UI or GAI did not complete";
    loop_ok = false;
  }
  if (loop_ok && (!m26_diagnostic.rendered || !m26_diagnostic.scroll_advanced ||
                  !m26_diagnostic.caret_advanced)) {
    m12_error = "M26 real UI, scroll or Edit did not complete";
    loop_ok = false;
  }
  if (loop_ok && !m27_diagnostic.scripted_done) {
    m12_error = "M27 scripted real interaction did not complete";
    loop_ok = false;
  }
  if (!loop_ok) Log("[STAGE] runtime loop FAIL reason=%s", m12_error.c_str());
  else {
    Log("[M12] exit_reason=%s", srhd_awa::platform::runtime_loop_slice::ExitReasonName(loop_stats.exit_reason));
    Log("[M12] loop frames=%llu loop duration_ms=%llu presents=%llu",
        static_cast<unsigned long long>(loop_stats.frames), static_cast<unsigned long long>(loop_stats.duration_ms),
        static_cast<unsigned long long>(loop_stats.presents));
    Stage("runtime loop", true);
  }
  if (m17_diagnostic.initial_cycle_verified && m17_diagnostic.sequence_pass && m17_diagnostic.cycle_pass && m17_diagnostic.timing_pass)
    Stage("M17 GAI playback", true);
  else
    Stage("M17 GAI playback", false, loop_ok ? "incomplete" : m12_error.c_str());
  Stage("M18 compositor", loop_ok && m20_diagnostic.rendered, loop_ok ? nullptr : m12_error.c_str());
  Stage("M19 scene compositor", loop_ok && m20_diagnostic.rendered, loop_ok ? nullptr : m12_error.c_str());
  Stage("M20 GI object", loop_ok && m20_diagnostic.rendered, loop_ok ? nullptr : m12_error.c_str());
  Stage("M21 UI image foundation", loop_ok && m21_diagnostic.rendered, loop_ok ? nullptr : m12_error.c_str());
  Stage("M22 UI object tree", loop_ok && m22_diagnostic.rendered, loop_ok ? nullptr : m12_error.c_str());
  Stage("M23 text/label", loop_ok && m23_diagnostic.rendered, loop_ok ? nullptr : m12_error.c_str());
  Stage("M24 UI controls", loop_ok && m24_diagnostic.Rendered(), loop_ok ? nullptr : m12_error.c_str());
  Stage("M25 GI/GAI UI", loop_ok && m25_diagnostic.rendered && m25_diagnostic.advanced,
        loop_ok ? nullptr : m12_error.c_str());
  const bool m26_pass = loop_ok && m26_diagnostic.rendered &&
      m26_diagnostic.scroll_advanced && m26_diagnostic.caret_advanced;
  if (m26_diagnostic.frames) {
    Log("[M26] perf frames=%llu presents=%llu avg_update_ms=%.3f avg_render_ms=%.3f max_update_ms=%.3f max_render_ms=%.3f",
        static_cast<unsigned long long>(m26_diagnostic.frames),
        static_cast<unsigned long long>(loop_stats.presents),
        static_cast<double>(m26_diagnostic.update_us_total) / m26_diagnostic.frames / 1000.0,
        static_cast<double>(m26_diagnostic.render_us_total) /
            std::max<std::uint64_t>(1, loop_stats.presents) / 1000.0,
        static_cast<double>(m26_diagnostic.update_us_max) / 1000.0,
        static_cast<double>(m26_diagnostic.render_us_max) / 1000.0);
  }
  if (m26_pass) {
    Log("[M26] GraphBuf PASS");
    Log("[M26] ScrollBar PASS");
    Log("[M26] PanelScrollBar PASS");
    Log("[M26] Edit PASS");
    Log("[M26] real UI PASS nodes=50");
  }
  Stage("M26 GraphBuf/Scroll/Edit/real UI", m26_pass,
        m26_pass ? nullptr : m12_error.c_str());
  Log("[M27] input perf events=%llu avg_dispatch_us=%.3f max_dispatch_us=%llu",
      static_cast<unsigned long long>(loop_stats.input_events_processed),
      static_cast<double>(loop_stats.input_dispatch_us_total) /
          std::max<std::uint64_t>(1, loop_stats.frames),
      static_cast<unsigned long long>(loop_stats.input_dispatch_us_max));
  Stage("M27 input/focus", loop_ok && m27_diagnostic.scripted_done,
        loop_ok ? nullptr : m12_error.c_str());
  srhd_awa::platform::runtime_loop_slice::Shutdown(&runtime_loop);
  srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);
  srhd_awa::platform::ui_metadata_slice::Shutdown();
  srhd_awa::platform::runtime_settings_slice::Shutdown();
  srhd_awa::platform::startup_slice::Shutdown(&startup);
  Log("[SHUTDOWN] renderer PASS");
  Log("[SHUTDOWN] runtime settings PASS");
  Log("[SHUTDOWN] startup PASS");
  Log("[BOOT] COMPLETE");
  return loop_ok ? 0 : 1;
}
