// Host fixture for the unchanged original TGraphBufGR::RescaleWithAspect.
// The small stubs are deliberately aborting on the D3D branch: the Switch
// source override selects the software branch for this UI background.
#include "layout/GR_GraphBuf.hpp"
#include "types/GR_gi.hpp"
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
void Ex_OKGF_Convert_8888to565(void* dest, std::int32_t dest_pitch,
                                  std::int32_t dest_x, std::int32_t dest_y,
                                  void* source, std::int32_t source_pitch,
                                  std::int32_t source_x, std::int32_t source_y,
                                  std::int32_t width, std::int32_t height) {
  ::OKGF_Convert_8888to565(dest, dest_pitch, dest_x, dest_y, source,
                           source_pitch, source_x, source_y, width, height);
}
void Ex_OKGF_Convert565toBGRA(void* source, std::int32_t source_pitch,
                                 void* dest, std::int32_t dest_pitch,
                                 std::int32_t width, std::int32_t height) {
  ::OKGF_Convert565toBGRA(source, source_pitch, dest, dest_pitch, width, height);
}
// Other GI formats are outside this fixture. Fail immediately if the
// installed Format-0 resource unexpectedly reaches one of their decoders.
#define E2E5_UNSUPPORTED_GI_DECODER(name) \
  void name(void*, std::int32_t, void*) { std::abort(); }
E2E5_UNSUPPORTED_GI_DECODER(Ex_OKGR_TransBuf_Draw_RGBA)
E2E5_UNSUPPORTED_GI_DECODER(Ex_OKGR_AlphaBuf_Draw_RGBA)
E2E5_UNSUPPORTED_GI_DECODER(Ex_OKGR_TransAlphaBuf_Draw_RGBA)
E2E5_UNSUPPORTED_GI_DECODER(Ex_OKGR_AlphaIndexed_Draw_RGBA)
E2E5_UNSUPPORTED_GI_DECODER(Ex_OKGR_AlphaIndexed_AlphaDraw_RGBA)
E2E5_UNSUPPORTED_GI_DECODER(Ex_OKGR_F5_DrawRGBA)
E2E5_UNSUPPORTED_GI_DECODER(Ex_OKGR_F6_DrawRGBA)
#undef E2E5_UNSUPPORTED_GI_DECODER
void ExpandPaletteToBgra(void*, std::int32_t, std::uint32_t, std::uint32_t,
                         void*, std::int32_t, void*) { std::abort(); }
}  // namespace GR_Main

int main(int argc, char** argv) {
  if (argc != 6) return 2;
  std::ifstream stream(argv[1], std::ios::binary);
  if (!stream) return 3;
  const std::vector<std::uint8_t> pixels(std::istreambuf_iterator<char>{stream}, {});
  constexpr std::int32_t source_width = 1920, source_height = 1200;
  if (pixels.size() != static_cast<std::size_t>(source_width) * source_height * 4) return 4;
  std::ifstream encoded_stream(argv[4], std::ios::binary);
  if (!encoded_stream) return 5;
  std::vector<std::uint8_t> encoded(std::istreambuf_iterator<char>{encoded_stream}, {});
  if (encoded.size() < 96 || encoded.size() > 32u * 1024u * 1024u) return 13;
  GR_gi::TgiGR source{};
  source.LoadRawGiBytes(encoded.data(), static_cast<std::int32_t>(encoded.size()));
  GR_GraphBuf::TGraphBufGR graph{};
  graph.UseTexture = false;
  graph.UsesTextureStorage = false;
  source.DecodeToGraphBuf(&graph, false);
  if (graph.Width != source_width || graph.Height != source_height ||
      graph.PitchBytes != source_width * 4 || graph.Pixels == nullptr) return 14;
  std::uint64_t decoded_hash = 0xcbf29ce484222325ull;
  const auto* decoded = static_cast<const std::uint8_t*>(graph.Pixels);
  for (std::size_t i = 0; i < pixels.size(); ++i)
    decoded_hash = (decoded_hash ^ decoded[i]) * 0x100000001b3ull;
  std::uint64_t expected_decoded{};
  try { expected_decoded = std::stoull(argv[5], nullptr, 16); }
  catch (...) { return 15; }
  if (decoded_hash != expected_decoded ||
      std::memcmp(graph.Pixels, pixels.data(), pixels.size()) != 0) return 16;
  source.ClearData();
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
  if (hash != expected) return 8;
  GR_gi::TgiGR image{};
  image.CreateFromGraphBuf(&graph, 1);
  constexpr std::size_t packed_bytes = 1280u * 720u * 2u;
  const auto* plane = image.GetPlane(0);
  if (image.DataSize != static_cast<std::int32_t>(96 + packed_bytes) ||
      image.Header->Format != 0 || plane->DataOffset != 96 ||
      plane->DataSize != static_cast<std::int32_t>(packed_bytes)) return 9;
  std::uint64_t packed_hash = 0xcbf29ce484222325ull;
  const auto* packed = static_cast<const std::uint8_t*>(image.Data) + plane->DataOffset;
  for (std::size_t i = 0; i < packed_bytes; ++i)
    packed_hash = (packed_hash ^ packed[i]) * 0x100000001b3ull;
  std::uint64_t restored_hash = 0xcbf29ce484222325ull;
  const auto* restored = static_cast<const std::uint8_t*>(graph.Pixels);
  for (std::size_t i = 0; i < 1280u * 720u * 4u; ++i)
    restored_hash = (restored_hash ^ restored[i]) * 0x100000001b3ull;
  if (restored_hash != hash) return 12;
  std::uint64_t expected_packed{};
  try { expected_packed = std::stoull(argv[3], nullptr, 16); }
  catch (...) { return 10; }
  std::printf("original_decode=PASS fnv64=%016llx "
              "original_graphbuf=PASS width=%d height=%d pitch=%d fnv64=%016llx "
              "original_gi=PASS bytes=%zu fnv64=%016llx\n",
              static_cast<unsigned long long>(decoded_hash),
              graph.Width, graph.Height, graph.PitchBytes,
              static_cast<unsigned long long>(hash), packed_bytes,
              static_cast<unsigned long long>(packed_hash));
  image.ClearData();
  graph.Clear();
  return packed_hash == expected_packed ? 0 : 11;
}
