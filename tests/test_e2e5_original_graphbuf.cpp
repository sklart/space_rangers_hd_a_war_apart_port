// Host fixture for the unchanged original TGraphBufGR::RescaleWithAspect.
// The small stubs are deliberately aborting on the D3D branch: the Switch
// source override selects the software branch for this UI background.
#include "layout/GR_GraphBuf.hpp"
#include "types/EC_Buf.hpp"
#if defined(E2E5_ORIGINAL_CACHEGI)
#include "types/EC_CacheGI.hpp"
#include "units/EC_CacheGI.hpp"
#endif
#include "types/GR_gi.hpp"
#include "units/ClassesImports.hpp"
#include "units/EC_Buf.hpp"
#include "units/EC_Mem.hpp"
#include "units/GR_DX.hpp"
#include "units/GR_GraphBuf.hpp"
#include "units/GR_Main.hpp"
#include "units/GR_gi.hpp"
#include "okgf.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace EC_Mem {
void* AllocEC(std::int32_t bytes) { return bytes > 0 ? std::malloc(bytes) : nullptr; }
void FreeEC(void* pointer) { std::free(pointer); }
void* ReAllocREC(void* pointer, std::int32_t bytes) {
  return bytes > 0 ? std::realloc(pointer, bytes) : nullptr;
}
}  // namespace EC_Mem

namespace ClassesImports {
Types::TRect Rect(std::int32_t left, std::int32_t top,
                  std::int32_t right, std::int32_t bottom) {
  return {left, top, right, bottom};
}
#if defined(E2E5_ORIGINAL_CACHEGI)
Types::TPoint Point(std::int32_t x, std::int32_t y) { return {x, y}; }
#endif
}  // namespace ClassesImports

#if defined(E2E5_ORIGINAL_CACHEGI)
namespace GR_Main {
std::int32_t GameScreenWidth = 1280;
std::int32_t GameScreenHeight = 720;
std::int32_t ExtraScreenWidth = 256;
std::int32_t ExtraScreenHeight = -48;
std::int32_t E2E5LogTextCalls = 0;
std::int32_t E2E5LogLineCalls = 0;
pas::WideString GiResourceSuffix() { return pas::WideString(u"2"); }
void AppendLogTextThreadSafe(const pas::AnsiString& text) {
  if (std::string(reinterpret_cast<const char*>(text.pchar()), text.length()) ==
      "Rescaling Bm.FormGameSet2.2bg... ") ++E2E5LogTextCalls;
}
void AppendLogLineThreadSafe(const pas::AnsiString& text) {
  if (std::string(reinterpret_cast<const char*>(text.pchar()), text.length()) == "ok")
    ++E2E5LogLineCalls;
}
std::int32_t Ex_OKGR_TransBuf_BuildFromRGBA_16(void*, std::int32_t,
                                                 std::int32_t, std::int32_t, void*) {
  std::abort();
}
std::int32_t Ex_OKGR_TransAlphaBuf_BuildFromRGBA_16(void*, std::int32_t,
                                                      std::int32_t, std::int32_t, void*) {
  std::abort();
}
std::int32_t Ex_OKGR_AlphaBuf_BuildFromRGBA(void*, std::int32_t,
                                             std::int32_t, std::int32_t, void*) {
  std::abort();
}
}  // namespace GR_Main
#endif

namespace GR_DX {
void GR_CreateTexture(std::int32_t, std::int32_t, std::uint32_t,
                      std::uint32_t, Direct3D9::IDirect3DTexture9&) {
  std::abort();
}
#if defined(E2E5_ORIGINAL_CACHEGI)
void FreeTextureCache(TTextureGR*) { std::abort(); }
#endif
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
  auto* source_buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
  source_buffer->AddBytes(encoded.data(), static_cast<std::int32_t>(encoded.size()));
#if defined(E2E5_ORIGINAL_CACHEGI)
  {
  EC_CacheGI::TCGiEC::ApplyWideScreenLayoutFixups(
      source_buffer, pas::WideString(u"Bm.FormGameSet2.2bg"));
  constexpr std::size_t packed_bytes = 1280u * 720u * 2u;
  if (source_buffer->DataSize != static_cast<std::int32_t>(96 + packed_bytes) ||
      GR_Main::E2E5LogTextCalls != 1 || GR_Main::E2E5LogLineCalls != 1) return 21;
  std::uint64_t cachegi_hash = 0xcbf29ce484222325ull;
  const auto* cachegi_bytes = static_cast<const std::uint8_t*>(source_buffer->Data) + 96;
  for (std::size_t i = 0; i < packed_bytes; ++i)
    cachegi_hash = (cachegi_hash ^ cachegi_bytes[i]) * 0x100000001b3ull;
  std::uint64_t expected_cachegi{};
  try { expected_cachegi = std::stoull(argv[3], nullptr, 16); }
  catch (...) { return 22; }
  if (cachegi_hash != expected_cachegi) {
    std::fprintf(stderr, "original CacheGI RGB565 fnv64=%016llx expected=%016llx\n",
                 static_cast<unsigned long long>(cachegi_hash),
                 static_cast<unsigned long long>(expected_cachegi));
    return 23;
  }
  auto* load_buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
  load_buffer->AddBytes(encoded.data(), static_cast<std::int32_t>(encoded.size()));
  auto* cache = pas::construct_call<EC_CacheGI::TCGiEC>(EC_CacheGI::TCGiEC_Create);
  cache->CacheKey = pas::WideString(u"Bm.FormGameSet2.2bg");
  cache->LoadFromConfigBuffer(load_buffer, pas::WideString());
  if (GR_Main::E2E5LogTextCalls != 2 || GR_Main::E2E5LogLineCalls != 2 ||
      cache->ResidentBytes != static_cast<std::int32_t>(96 + packed_bytes) ||
      cache->Image == nullptr || cache->Image->DataSize != cache->ResidentBytes ||
      load_buffer->DataSize != cache->ResidentBytes ||
      std::memcmp(cache->Image->Data, source_buffer->Data,
                  static_cast<std::size_t>(cache->ResidentBytes)) != 0 ||
      std::memcmp(load_buffer->Data, source_buffer->Data,
                  static_cast<std::size_t>(cache->ResidentBytes)) != 0) return 24;
  load_buffer->Clear();
  pas::free(load_buffer);
  pas::free(cache);
  source_buffer->Clear();
  pas::free(source_buffer);
  std::printf("original_cachegi=PASS bytes=%zu fnv64=%016llx "
              "original_cachegi_load=PASS log_start=PASS log_done=PASS "
              "lifecycle=PASS\n",
              packed_bytes, static_cast<unsigned long long>(cachegi_hash));
  return 0;
  }
#endif
  auto* source = pas::construct_call<GR_gi::TgiGR>(GR_gi::TgiGR_Create);
  source->LoadRawGiFromBuffer(source_buffer);
  source_buffer->Clear();
  pas::free(source_buffer);
  auto* graph = pas::construct_call<GR_GraphBuf::TGraphBufGR>(
      GR_GraphBuf::TGraphBufGR_Create, false);
  source->DecodeToGraphBuf(graph, false);
  if (graph->Width != source_width || graph->Height != source_height ||
      graph->PitchBytes != source_width * 4 || graph->Pixels == nullptr) return 14;
  std::uint64_t decoded_hash = 0xcbf29ce484222325ull;
  const auto* decoded = static_cast<const std::uint8_t*>(graph->Pixels);
  for (std::size_t i = 0; i < pixels.size(); ++i)
    decoded_hash = (decoded_hash ^ decoded[i]) * 0x100000001b3ull;
  std::uint64_t expected_decoded{};
  try { expected_decoded = std::stoull(argv[5], nullptr, 16); }
  catch (...) { return 15; }
  if (decoded_hash != expected_decoded ||
      std::memcmp(graph->Pixels, pixels.data(), pixels.size()) != 0) return 16;
  source->ClearData();
  pas::free(source);
  constexpr std::size_t output_bytes = 1280u * 720u * 4u;
  std::vector<std::uint8_t> direct(output_bytes);
  ::OKGF_Rescale(direct.data(), 1280, 720, 5120,
                 static_cast<std::uint8_t*>(graph->Pixels) + 60 * graph->PitchBytes,
                 1920, 1080, graph->PitchBytes, 4, OKGF_RESCALE_LANCZOS3);
  // Exercise the effective Switch override: the original CacheGI method
  // calls RescaleRGBA_HW, whose disposable body delegates to the CPU path.
  graph->RescaleRGBA_HW(1280, 720, true, 1, 1);
  if (graph->Width != 1280 || graph->Height != 720 || graph->PitchBytes != 5120 ||
      graph->Pixels == nullptr) return 6;
  std::uint64_t hash = 0xcbf29ce484222325ull;
  const auto* output = static_cast<const std::uint8_t*>(graph->Pixels);
  for (std::size_t i = 0; i < output_bytes; ++i)
    hash = (hash ^ output[i]) * 0x100000001b3ull;
  if (std::memcmp(output, direct.data(), output_bytes) != 0) {
    std::fprintf(stderr, "original GraphBuf differs from direct same-architecture OKGF; fnv64=%016llx\n",
                 static_cast<unsigned long long>(hash));
    return 19;
  }
  std::uint64_t expected{};
  if (std::strcmp(argv[2], "-") != 0) {
    try { expected = std::stoull(argv[2], nullptr, 16); }
    catch (...) { return 7; }
    if (hash != expected) {
      std::fprintf(stderr, "rescale fnv64=%016llx expected=%016llx\n",
                   static_cast<unsigned long long>(hash),
                   static_cast<unsigned long long>(expected));
      return 8;
    }
  }
  auto* image = pas::construct_call<GR_gi::TgiGR>(GR_gi::TgiGR_Create);
  image->CreateFromGraphBuf(graph, 1);
  constexpr std::size_t packed_bytes = 1280u * 720u * 2u;
  const auto* plane = image->GetPlane(0);
  if (image->DataSize != static_cast<std::int32_t>(96 + packed_bytes) ||
      image->Header->Format != 0 || plane->DataOffset != 96 ||
      plane->DataSize != static_cast<std::int32_t>(packed_bytes)) return 9;
  std::uint64_t packed_hash = 0xcbf29ce484222325ull;
  const auto* packed = static_cast<const std::uint8_t*>(image->Data) + plane->DataOffset;
  for (std::size_t i = 0; i < packed_bytes; ++i)
    packed_hash = (packed_hash ^ packed[i]) * 0x100000001b3ull;
  std::vector<std::uint8_t> rgba(direct);
  for (std::size_t i = 0; i < rgba.size(); i += 4) {
    const auto blue = rgba[i];
    rgba[i] = rgba[i + 2];
    rgba[i + 2] = blue;
  }
  std::vector<std::uint8_t> direct_packed(packed_bytes);
  ::OKGF_Convert_8888to565(direct_packed.data(), 2560, 0, 0,
                            rgba.data(), 5120, 0, 0, 1280, 720);
  if (std::memcmp(packed, direct_packed.data(), packed_bytes) != 0) {
    std::fprintf(stderr, "original GI repack differs from direct same-architecture OKGF; fnv64=%016llx\n",
                 static_cast<unsigned long long>(packed_hash));
    return 20;
  }
  std::uint64_t restored_hash = 0xcbf29ce484222325ull;
  const auto* restored = static_cast<const std::uint8_t*>(graph->Pixels);
  for (std::size_t i = 0; i < 1280u * 720u * 4u; ++i)
    restored_hash = (restored_hash ^ restored[i]) * 0x100000001b3ull;
  if (restored_hash != hash) return 12;
  std::uint64_t expected_packed{};
  if (std::strcmp(argv[3], "-") != 0) {
    try { expected_packed = std::stoull(argv[3], nullptr, 16); }
    catch (...) { return 10; }
    if (packed_hash != expected_packed) {
      std::fprintf(stderr, "RGB565 fnv64=%016llx expected=%016llx\n",
                   static_cast<unsigned long long>(packed_hash),
                   static_cast<unsigned long long>(expected_packed));
      return 11;
    }
  }
  auto* repacked_buffer = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
  repacked_buffer->AddBytes(image->Data, image->DataSize);
  image->ClearData();
  pas::free(image);
  if (repacked_buffer->DataSize != static_cast<std::int32_t>(96 + packed_bytes)) return 17;
  const auto* buffered = static_cast<const std::uint8_t*>(repacked_buffer->Data) + 96;
  std::uint64_t buffered_hash = 0xcbf29ce484222325ull;
  for (std::size_t i = 0; i < packed_bytes; ++i)
    buffered_hash = (buffered_hash ^ buffered[i]) * 0x100000001b3ull;
  if (buffered_hash != packed_hash) return 18;
  const auto final_width = graph->Width;
  const auto final_height = graph->Height;
  const auto final_pitch = graph->PitchBytes;
  repacked_buffer->Clear();
  pas::free(repacked_buffer);
  graph->Clear();
  pas::free(graph);
  std::printf("original_decode=PASS fnv64=%016llx "
              "original_graphbuf=PASS width=%d height=%d pitch=%d fnv64=%016llx "
              "original_gi=PASS bytes=%zu fnv64=%016llx buffer=PASS "
              "effective_switch_rescale_override=PASS lifecycle=PASS\n",
              static_cast<unsigned long long>(decoded_hash),
              final_width, final_height, final_pitch,
              static_cast<unsigned long long>(hash), packed_bytes,
              static_cast<unsigned long long>(packed_hash));
  return 0;
}
