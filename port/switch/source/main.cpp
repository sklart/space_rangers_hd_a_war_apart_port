#include <SDL2/SDL.h>
#include <switch.h>
#include <okgf.h>
#include "units/CrcUnit.hpp"
#include "units/System.hpp"
#include "units/SystemImports.hpp"
#include "filesystem.hpp"
#include "package.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
constexpr const char* kLogPath = "sdmc:/switch/space-rangers-hd-a-war-apart/port.log";
constexpr const char* kDefaultGameRoot = "sdmc:/switch/space-rangers-hd-a-war-apart/game";

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
  Log("[PACKAGE] PASS folders=%lu files=%lu entries=%lu depth=%lu selected=DATA/Asteroid/00.gai kind=%ld size=%lu crc32=%08lx",
      static_cast<unsigned long>(tree.folders), static_cast<unsigned long>(tree.files),
      static_cast<unsigned long>(tree.entries), static_cast<unsigned long>(tree.max_depth),
      static_cast<long>(entry->kind), static_cast<unsigned long>(payload.size()), static_cast<unsigned long>(payload_crc));
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  Log("[BOOT] BEGIN Space Rangers HD: A War Apart");
  Log("[BOOT] runtime units=CrcUnit,System,SystemImports (SpaceRangersHD_CPP)");
  SystemImports::Randomize();
  Log("[GAME] PASS SystemImports::Randomize RandSeed=%lu", static_cast<unsigned long>(System::RandSeed));
  const char* game_root = GameRoot(argc, argv);
  Log("[FILESYSTEM] BEGIN game-root=%s", game_root);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    Log("[PLATFORM] FAIL SDL_Init=%s", SDL_GetError());
    return 1;
  }
  Log("[PLATFORM] PASS SDL2 initialized");

  SDL_Window* window = SDL_CreateWindow("Space Rangers HD", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN);
  if (!window) {
    Log("[PLATFORM] FAIL SDL_CreateWindow=%s", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  Log("[PLATFORM] PASS presentation=1280x720");
  std::uint16_t framebuffer[4]{};
  OKGR_Fill_WORD(framebuffer, static_cast<std::int32_t>(sizeof(framebuffer)), 2, 2, 0x07e0);
  if (framebuffer[0] != 0x07e0 || framebuffer[3] != 0x07e0) {
    Log("[OKGF] FAIL OKGR_Fill_WORD framebuffer validation");
  } else {
    Log("[OKGF] PASS portable renderer CPU framebuffer 2x2");
  }
  const bool resource_ok = ReadRequiredAsset(game_root);
  Log("[FILESYSTEM] %s game-root", resource_ok ? "PASS" : "FAIL");
  Log("[GAME] %s real C++ runtime bootstrap", resource_ok ? "PASS" : "WAITING_FOR_PACKAGE_LOADER");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
