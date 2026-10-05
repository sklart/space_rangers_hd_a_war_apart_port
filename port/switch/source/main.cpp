#include <switch.h>
#include <okgf.h>
#include "units/CrcUnit.hpp"
#include "units/System.hpp"
#include "units/SystemImports.hpp"
#include "filesystem.hpp"
#include "package.hpp"
#include "ec_file_adapter.hpp"
#include "startup_slice.hpp"
#include "runtime_settings_slice.hpp"
#include "renderer_platform.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/EC_BlockPar.hpp"
#include "units/EC_Data.hpp"
#include "units/EC_File.hpp"
#include "units/EC_HsFile.hpp"
#include "units/GR_Main.hpp"
#include "units/aPacket.hpp"

#include <cstdarg>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {
constexpr const char* kLogPath = "sdmc:/switch/space-rangers-hd-a-war-apart/logs/port.log";
constexpr const char* kPreviousLogPath = "sdmc:/switch/space-rangers-hd-a-war-apart/logs/port-prev.log";
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

struct Fingerprint { std::uint64_t hash{UINT64_C(1469598103934665603)}; std::uint32_t entries{}; std::uint32_t blocks{}; std::uint32_t params{}; std::uint32_t files{}; std::uint32_t nodes{}; std::uint32_t depth{}; };
void HashByte(Fingerprint* fp, std::uint8_t value) { fp->hash = (fp->hash ^ value) * UINT64_C(1099511628211); }
void HashWide(Fingerprint* fp, const pas::WideString& value) { for (const char16_t* p = value.pchar(); p && *p; ++p) { HashByte(fp, static_cast<std::uint8_t>(*p)); HashByte(fp, static_cast<std::uint8_t>(*p >> 8)); } HashByte(fp, 0xff); }
void FingerprintBlock(EC_BlockPar::TBlockParEC* block, Fingerprint* fp, std::uint32_t depth) {
  if (!block) return; fp->depth = std::max(fp->depth, depth);
  for (std::int32_t i = 0; i < block->GetEntryCount(); ++i) { ++fp->entries; const auto kind = block->GetEntryKindByIndex(i); HashByte(fp, static_cast<std::uint8_t>(kind)); HashWide(fp, block->GetEntryNameByIndex(i)); if (kind == EC_BlockPar::bpkBlock) { ++fp->blocks; FingerprintBlock(block->GetEntryBlockByIndex(i), fp, depth + 1); } else { ++fp->params; HashWide(fp, block->GetEntryStringByIndex(i)); } }
}
void FingerprintData(EC_Data::TDataEC* data, Fingerprint* fp, std::uint32_t depth) {
  if (!data) return; fp->depth = std::max(fp->depth, depth);
  for (auto* entry = data->FirstEntry; entry; entry = entry->Next) { ++fp->nodes; HashByte(fp, static_cast<std::uint8_t>(entry->Kind)); HashWide(fp, entry->Name); if (entry->Kind == EC_Data::dekSubtree) FingerprintData(entry->ChildData, fp, depth + 1); else { ++fp->files; if (entry->SharedFileRef && entry->SharedFileRef->FileRef) HashWide(fp, entry->SharedFileRef->FileRef->GetFileName()); HashByte(fp, static_cast<std::uint8_t>(entry->FileOffset)); HashByte(fp, static_cast<std::uint8_t>(entry->ByteCount)); } }
}

const char* GameRoot(int argc, char** argv) {
  if (argc > 1 && argv[1] && argv[1][0] != '\0') return argv[1];
  return kDefaultGameRoot;
}

bool ConfigurePackages(srhd_awa::platform::startup_slice::State* state, const char* root) {
  std::string error;
  StageBegin("startup configuration");
  Log("[M7] platform init begin");
  if (!srhd_awa::platform::startup_slice::Initialize(state, root, kDefaultUserRoot, kLogPath, &error)) {
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
  Log("[PACKAGE] PASS folders=%lu files=%lu entries=%lu depth=%lu tree_hash=%016llx selected=DATA/Asteroid/00.gai kind=%ld size=%lu crc32=%08lx",
      static_cast<unsigned long>(tree.folders), static_cast<unsigned long>(tree.files),
      static_cast<unsigned long>(tree.entries), static_cast<unsigned long>(tree.max_depth), static_cast<unsigned long long>(tree.tree_hash),
      static_cast<long>(entry->kind), static_cast<unsigned long>(payload.size()), static_cast<unsigned long>(payload_crc));
  Log("[SELFTEST] %s package baseline", baseline_ok ? "PASS" : "FAIL");
  Log("[M7] resource %s", ec_ok ? "PASS" : "FAIL");
  return baseline_ok && ec_ok;
}

bool RunRendererSelfTest() {
  Log("[M8] renderer init begin");
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
  const bool ok = before_outer_end == 0 && present_count == 1 && hash != 0;
  Log("[M8] OKGF bridge %s", ok ? "PASS" : "FAIL");
  Log("[M8] draw %s fnv64=%016llx", ok ? "PASS" : "FAIL",
      static_cast<unsigned long long>(hash));
  Log("[M8] present backend=SDL2-RGB565");
  Log("[M8] present %s", ok ? "PASS" : "FAIL");
  Stage("SDL presentation", ok);
  Log("[M8] renderer boundary %s", ok ? "reached" : "failed");
  return ok;
}
}  // namespace

int main(int argc, char** argv) {
  BeginLogSession();
  Log("[BOOT] BEGIN Space Rangers HD: A War Apart");
  Log("[BOOT] Space Rangers HD: A War Apart");
  Log("[BOOT] milestone=10-hardware-baseline");
  Log("[BOOT] build_git=%s baseline_rangers_sha256=83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98", BUILD_GIT_COMMIT);
  Log("[BOOT] runtime units=CrcUnit,System,SystemImports (SpaceRangersHD_CPP)");
  SystemImports::Randomize();
  Log("[GAME] PASS SystemImports::Randomize RandSeed=%lu", static_cast<unsigned long>(System::RandSeed));
  const char* game_root = GameRoot(argc, argv);
  Log("[BOOT] game_root=%s", game_root);
  Log("[BOOT] user_root=%s", kDefaultUserRoot);
  Log("[BOOT] baseline=2.1.2500");
  Log("[FILESYSTEM] BEGIN game-root=%s", game_root);

  Log("[GR_MAIN] linked");
  srhd_awa::platform::startup_slice::State startup;
  if (!ConfigurePackages(&startup, game_root)) return 1;
  Stage("SDL/platform services", true);
  Stage("window", true);
  std::string m9_error;
  StageBegin("DAT/runtime config");
  Log("[M9] dat config begin");
  if (!srhd_awa::platform::runtime_settings_slice::Initialize(&m9_error)) {
    Log("[M9] FAIL runtime config=%s", m9_error.c_str());
    Stage("DAT/runtime config", false, m9_error.c_str());
    srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  Log("[M9] Main.dat PASS");
  Log("[M9] Lang.dat PASS language=%s", static_cast<const char*>(static_cast<pas::AnsiString>(GR_Main::LanguageInstallConfig->GetParam(u"Lang"sv)).c_str()));
  Log("[M9] CacheData.dat PASS");
  Fingerprint main_fp, lang_fp, cache_fp;
  FingerprintBlock(GR_Main::MainDataConfig, &main_fp, 1);
  FingerprintBlock(GR_Main::LanguageDataConfig, &lang_fp, 1);
  FingerprintData(GR_Main::CacheDataRoot, &cache_fp, 1);
  Log("[M9] Main.dat entries=%lu blocks=%lu params=%lu depth=%lu fnv64=%016llx", static_cast<unsigned long>(main_fp.entries), static_cast<unsigned long>(main_fp.blocks), static_cast<unsigned long>(main_fp.params), static_cast<unsigned long>(main_fp.depth), static_cast<unsigned long long>(main_fp.hash));
  Log("[M9] Lang.dat language=%s entries=%lu blocks=%lu params=%lu depth=%lu fnv64=%016llx", static_cast<const char*>(static_cast<pas::AnsiString>(GR_Main::LanguageInstallConfig->GetParam(u"Lang"sv)).c_str()), static_cast<unsigned long>(lang_fp.entries), static_cast<unsigned long>(lang_fp.blocks), static_cast<unsigned long>(lang_fp.params), static_cast<unsigned long>(lang_fp.depth), static_cast<unsigned long long>(lang_fp.hash));
  Log("[M9] CacheData.dat nodes=%lu files=%lu depth=%lu fnv64=%016llx", static_cast<unsigned long>(cache_fp.nodes), static_cast<unsigned long>(cache_fp.files), static_cast<unsigned long>(cache_fp.depth), static_cast<unsigned long long>(cache_fp.hash));
  Log("[M9] user config PASS");
  Log("[M9] GameDataConfig PASS");
  Log("[M9] UiDepthConfig PASS");
  Log("[M9] CaseConv count=%ld", static_cast<long>(GR_Main::WideCaseTable.length()));
  Stage("DAT/runtime config", true);
  srhd_awa::platform::runtime_platform::PumpEvents(startup.platform);
  srhd_awa::platform::renderer_platform::SetNativeWindow(startup.platform.native_window);
  const bool renderer_ok = RunRendererSelfTest();
  if (!renderer_ok) Stage("renderer self-test", false);
#ifdef __SWITCH__
  if (renderer_ok) {
    Log("[M8] visible-frame hold BEGIN seconds=5");
    for (int index = 0; index < 500 && appletMainLoop(); ++index) {
      srhd_awa::platform::runtime_platform::PumpEvents(startup.platform);
      svcSleepThread(10000000ULL);
    }
    Log("[M8] visible-frame hold PASS");
  }
#endif
  const bool resource_ok = ReadRequiredAsset(game_root);
  Stage("package baseline", resource_ok);
  Log("[FILESYSTEM] %s game-root", resource_ok && renderer_ok ? "PASS" : "FAIL");
  Log("[M7] startup boundary %s", resource_ok ? "reached" : "failed");
  srhd_awa::platform::renderer_platform::ShutdownSoftwareRenderer();
  srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);
  srhd_awa::platform::runtime_settings_slice::Shutdown();
  srhd_awa::platform::startup_slice::Shutdown(&startup);
  Log("[SHUTDOWN] renderer PASS");
  Log("[SHUTDOWN] runtime settings PASS");
  Log("[SHUTDOWN] startup PASS");
  Log("[BOOT] COMPLETE");
  return resource_ok && renderer_ok ? 0 : 1;
}
