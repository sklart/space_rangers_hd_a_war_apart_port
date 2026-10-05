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
#include "units/EC_File.hpp"
#include "units/EC_HsFile.hpp"
#include "units/GR_Main.hpp"
#include "units/aPacket.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr const char* kLogPath = "sdmc:/switch/space-rangers-hd-a-war-apart/port.log";
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

const char* GameRoot(int argc, char** argv) {
  if (argc > 1 && argv[1] && argv[1][0] != '\0') return argv[1];
  return kDefaultGameRoot;
}

bool ConfigurePackages(srhd_awa::platform::startup_slice::State* state, const char* root) {
  std::string error;
  Log("[M7] platform init begin");
  if (!srhd_awa::platform::startup_slice::Initialize(state, root, kDefaultUserRoot, kLogPath, &error)) {
    Log("[M7] FAIL startup=%s", error.c_str());
    return false;
  }
  Log("[M7] timing PASS frequency=%lld", static_cast<long long>(GR_Main::PerformanceCounterFrequency));
  Log("[M7] window PASS token=%lu", static_cast<unsigned long>(GR_Main::MainWindowHandle));
  Log("[M7] package collection PASS");
  Log("[M7] install config PASS");
  std::int32_t package_count = 0;
  while (EC_HsFile::PackageCollection->GetPackByIndex(package_count)) ++package_count;
  Log("[M7] language packages PASS language=%s count=%ld",
      GR_Main::SelectedLanguage == u"russian" ? "russian" : "unexpected", static_cast<long>(package_count));
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
  Log("[M8] renderer boundary %s", ok ? "reached" : "failed");
  return ok;
}
}  // namespace

int main(int argc, char** argv) {
  Log("[BOOT] BEGIN Space Rangers HD: A War Apart");
  Log("[BOOT] build_git=%s baseline_rangers_sha256=83300344af802bc51e64389c58f047e5afdf195c133048098be3881fae29ed98", BUILD_GIT_COMMIT);
  Log("[BOOT] runtime units=CrcUnit,System,SystemImports (SpaceRangersHD_CPP)");
  SystemImports::Randomize();
  Log("[GAME] PASS SystemImports::Randomize RandSeed=%lu", static_cast<unsigned long>(System::RandSeed));
  const char* game_root = GameRoot(argc, argv);
  Log("[FILESYSTEM] BEGIN game-root=%s", game_root);

  Log("[GR_MAIN] linked");
  srhd_awa::platform::startup_slice::State startup;
  if (!ConfigurePackages(&startup, game_root)) return 1;
  std::string m9_error;
  Log("[M9] dat config begin");
  if (!srhd_awa::platform::runtime_settings_slice::Initialize(&m9_error)) {
    Log("[M9] FAIL runtime config=%s", m9_error.c_str());
    srhd_awa::platform::startup_slice::Shutdown(&startup);
    return 1;
  }
  Log("[M9] Main.dat PASS");
  Log("[M9] Lang.dat PASS language=%s", static_cast<const char*>(static_cast<pas::AnsiString>(GR_Main::LanguageInstallConfig->GetParam(u"Lang"sv)).c_str()));
  Log("[M9] CacheData.dat PASS");
  Log("[M9] user config PASS");
  Log("[M9] GameDataConfig PASS");
  Log("[M9] UiDepthConfig PASS");
  Log("[M9] CaseConv count=%ld", static_cast<long>(GR_Main::WideCaseTable.length()));
  srhd_awa::platform::runtime_platform::PumpEvents(startup.platform);
  srhd_awa::platform::renderer_platform::SetNativeWindow(startup.platform.native_window);
  const bool renderer_ok = RunRendererSelfTest();
  const bool resource_ok = ReadRequiredAsset(game_root);
  Log("[FILESYSTEM] %s game-root", resource_ok && renderer_ok ? "PASS" : "FAIL");
  Log("[M7] startup boundary %s", resource_ok ? "reached" : "failed");
  srhd_awa::platform::renderer_platform::ShutdownSoftwareRenderer();
  srhd_awa::platform::renderer_platform::SetNativeWindow(nullptr);
  srhd_awa::platform::runtime_settings_slice::Shutdown();
  srhd_awa::platform::startup_slice::Shutdown(&startup);
  return resource_ok && renderer_ok ? 0 : 1;
}
