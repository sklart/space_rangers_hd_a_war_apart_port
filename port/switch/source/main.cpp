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
#include "image_object.hpp"
#include "gi_object.hpp"
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
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
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
  srhd_awa::platform::image_object::PortableImageObject simple, trans, alpha;
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
  using srhd_awa::platform::image_object::Kind;
  diagnostic->simple.SetPackage(&diagnostic->package); diagnostic->simple.SetId("m21-simple");
  diagnostic->trans.SetPackage(&diagnostic->package); diagnostic->trans.SetId("m21-trans");
  diagnostic->alpha.SetPackage(&diagnostic->package); diagnostic->alpha.SetId("m21-alpha");
  if (!diagnostic->simple.Load(Kind::Simple, "DATA/PUMaps/01x120.jpg", "", error) ||
      !diagnostic->trans.Load(Kind::Trans, "DATA/Asteroid/Map.png", "", error) ||
      !diagnostic->alpha.Load(Kind::Alpha, "DATA/Planet2/120/01x120.psd", "", error)) return false;
  diagnostic->simple.SetPosition(12, height - diagnostic->simple.natural_height() - 12);
  diagnostic->trans.SetPosition(width - diagnostic->trans.natural_width() - 12, height - diagnostic->trans.natural_height() - 12);
  diagnostic->alpha.SetPosition(width / 2 - diagnostic->alpha.natural_width() / 2, 12);
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
  Log("[M21] static objects BEGIN count=3");
  const srhd_awa::platform::image_object::PortableImageObject* static_objects[]{&diagnostic->simple, &diagnostic->trans, &diagnostic->alpha};
  const char* static_kinds[]{"Simple", "Trans", "Alpha"};
  for (std::size_t index=0;index<3;++index) Log("[M21] object%zu kind=%s resource=%s size=%ldx%ld resident=%zu", index, static_kinds[index], static_objects[index]->resource().c_str(), static_cast<long>(static_objects[index]->natural_width()), static_cast<long>(static_objects[index]->natural_height()), static_objects[index]->resident_bytes());
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
  const srhd_awa::platform::okgf_rle_bridge::Rect clip{0, 0, framebuffer->Width, framebuffer->Height};
  if (!diagnostic->simple.DrawFramebuffer(target.pixels, target.width, target.height, target.pitch_pixels, clip, error) ||
      !diagnostic->trans.DrawFramebuffer(target.pixels, target.width, target.height, target.pitch_pixels, clip, error) ||
      !diagnostic->alpha.DrawFramebuffer(target.pixels, target.width, target.height, target.pitch_pixels, clip, error)) return false;
  if (!diagnostic->rendered) Log("[M21] static objects PASS count=3");
  if (!diagnostic->rendered) { diagnostic->rendered = true; Log("[M20] GI object PASS objects=3"); }
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
  M20FrameCallbacks m20_callbacks{&m17_diagnostic, &m20_diagnostic};
  srhd_awa::platform::runtime_loop_slice::SetFrameCallback(&runtime_loop, RunM17AndM20, &m20_callbacks);
  srhd_awa::platform::runtime_loop_slice::SetDrawCallback(&runtime_loop, DrawM20GiObjects, &m20_diagnostic);
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
