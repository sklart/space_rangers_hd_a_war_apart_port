#include <SDL2/SDL.h>
#include <switch.h>
#include "units/CrcUnit.hpp"
#include "units/System.hpp"
#include "units/SystemImports.hpp"
#include "filesystem.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

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
  const std::uint32_t crc = CrcUnit::ComputeCrc32(&package, static_cast<std::int32_t>(sizeof(package)));
  Log("[RESOURCE] PASS parser=EC_HsFile root size=%llu root=%lu entries=%lu record=%lu first=%s crc32=%08lx",
      static_cast<unsigned long long>(package.file_size), static_cast<unsigned long>(package.root_offset),
      static_cast<unsigned long>(package.entry_count), static_cast<unsigned long>(package.entry_record_size),
      package.first_entry_name, static_cast<unsigned long>(crc));
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
  const bool resource_ok = ReadRequiredAsset(game_root);
  Log("[FILESYSTEM] %s game-root", resource_ok ? "PASS" : "FAIL");
  Log("[GAME] %s real C++ runtime bootstrap", resource_ok ? "PASS" : "WAITING_FOR_PACKAGE_LOADER");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
