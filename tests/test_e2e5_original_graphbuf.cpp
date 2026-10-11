// Host fixture for the unchanged original TGraphBufGR::RescaleWithAspect.
// The small stubs are deliberately aborting on the D3D branch: the Switch
// source override selects the software branch for this UI background.
#include "layout/GR_GraphBuf.hpp"
#include "units/ClassesImports.hpp"
#include "units/EC_Mem.hpp"
#include "units/GR_DX.hpp"
#include "units/GR_Main.hpp"
#include "okgf.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace pas {
long double real_divide(long double left, long double right) { return left / right; }
std::int64_t real_round(long double value) {
  return static_cast<std::int64_t>(std::nearbyint(value));
}
}  // namespace pas

namespace EC_Struct {
void TObjectEx_Destroy(TObjectEx*) {}
void TObjectEx::p_destroy() {}
}  // namespace EC_Struct

namespace EC_Mem {
void* AllocEC(std::int32_t bytes) { return bytes > 0 ? std::malloc(bytes) : nullptr; }
void FreeEC(void* pointer) { std::free(pointer); }
}  // namespace EC_Mem

namespace ClassesImports {
Types::TRect Rect(std::int32_t left, std::int32_t top,
                  std::int32_t right, std::int32_t bottom) {
  return {left, top, right, bottom};
}
}  // namespace ClassesImports

namespace GR_DX {
void GR_CreateTexture(std::int32_t, std::int32_t, std::uint32_t,
                      std::uint32_t, Direct3D9::IDirect3DTexture9&) {
  std::abort();
}
}  // namespace GR_DX

namespace GR_Main {
void Ex_OKGF_Rescale(void* dest, std::int32_t width, std::int32_t height,
                     std::int32_t dest_pitch, void* source,
                     std::int32_t source_width, std::int32_t source_height,
                     std::int32_t source_pitch, std::int32_t bpp,
                     std::int32_t filter) {
  ::OKGF_Rescale(dest, width, height, dest_pitch, source,
                 source_width, source_height, source_pitch, bpp, filter);
}
}  // namespace GR_Main

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  std::ifstream stream(argv[1], std::ios::binary);
  if (!stream) return 3;
  const std::vector<std::uint8_t> pixels(std::istreambuf_iterator<char>{stream}, {});
  constexpr std::int32_t source_width = 1920, source_height = 1200;
  if (pixels.size() != static_cast<std::size_t>(source_width) * source_height * 4) return 4;
  auto* owned = std::malloc(pixels.size());
  if (!owned) return 5;
  std::memcpy(owned, pixels.data(), pixels.size());
  GR_GraphBuf::TGraphBufGR graph{};
  graph.Width = source_width;
  graph.Height = source_height;
  graph.PitchBytes = source_width * 4;
  graph.Pixels = owned;
  graph.StorageKind = 0;
  graph.BitsPerPixel = 32;
  graph.BytesPerPixel = 4;
  graph.UseTexture = false;
  graph.UsesTextureStorage = false;
  graph.RescaleWithAspect(1280, 720, true, 1, 1, OKGF_RESCALE_LANCZOS3);
  if (graph.Width != 1280 || graph.Height != 720 || graph.PitchBytes != 5120 ||
      graph.Pixels == nullptr) return 6;
  std::uint64_t hash = 0xcbf29ce484222325ull;
  const auto* output = static_cast<const std::uint8_t*>(graph.Pixels);
  for (std::size_t i = 0; i < 1280u * 720u * 4u; ++i)
    hash = (hash ^ output[i]) * 0x100000001b3ull;
  std::uint64_t expected{};
  try { expected = std::stoull(argv[2], nullptr, 16); }
  catch (...) { return 7; }
  std::printf("original_graphbuf=PASS width=%d height=%d pitch=%d fnv64=%016llx\n",
              graph.Width, graph.Height, graph.PitchBytes,
              static_cast<unsigned long long>(hash));
  graph.Clear();
  return hash == expected ? 0 : 8;
}
