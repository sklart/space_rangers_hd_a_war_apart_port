#include <SDL2/SDL.h>
#include <switch.h>

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

bool HasRequiredAsset(const char* root) {
  char path[512];
  std::snprintf(path, sizeof(path), "%s/DATA/common.pkg", root);
  std::FILE* asset = std::fopen(path, "rb");
  if (!asset) return false;
  std::fclose(asset);
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  Log("startup: Space Rangers HD: A War Apart Switch feasibility skeleton");
  Log("build: C++20, libnx, SDL2; stage=platform-init");
  const char* game_root = GameRoot(argc, argv);
  Log("game-root: %s", game_root);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    Log("fatal: SDL_Init: %s", SDL_GetError());
    return 1;
  }
  Log("SDL initialized");

  SDL_Window* window = SDL_CreateWindow("Space Rangers HD", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN);
  if (!window) {
    Log("fatal: SDL_CreateWindow: %s", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  Log("presentation initialized at 1280x720");
  Log("asset DATA/common.pkg: %s", HasRequiredAsset(game_root) ? "present" : "not found");
  Log("stop: feasibility skeleton; game runtime is intentionally not linked");
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
