// Reuse only the established original-GraphBuf fixture's imports and aborting
// D3D stubs. The test below supplies the real generated Ex_OKGF_Rescale body
// through a separate translation unit, not the fixture's direct mock.
#define main UnusedGraphbufFixtureMain
#define Ex_OKGF_Rescale UnusedDirectRescaleMock
#define Ex_OKGR_TransBuf_Draw_RGBA UnusedTransparentDecoder
#define Ex_OKGR_AlphaBuf_Draw_RGBA UnusedAlphaDecoder
#define Ex_OKGR_TransAlphaBuf_Draw_RGBA UnusedTransparentAlphaDecoder
#include "test_e2e5_original_graphbuf.cpp"
#undef Ex_OKGR_TransBuf_Draw_RGBA
#undef Ex_OKGR_AlphaBuf_Draw_RGBA
#undef Ex_OKGR_TransAlphaBuf_Draw_RGBA
#undef Ex_OKGF_Rescale
#undef main

#include <cerrno>
#include <algorithm>

namespace {
int fail_at{}, allocations{}, failures{};
}

// Only rescale.c is compiled with malloc/calloc redirected to these functions.
// Other allocations (source, expected pixels, exception objects) remain real.
extern "C" void* rescale_test_malloc(std::size_t bytes) {
  if (fail_at && ++allocations == fail_at) {
    ++failures; errno = ENOMEM; return nullptr;
  }
  return std::malloc(bytes);
}
extern "C" void* rescale_test_calloc(std::size_t count, std::size_t bytes) {
  if (fail_at && ++allocations == fail_at) {
    ++failures; errno = ENOMEM; return nullptr;
  }
  return std::calloc(count, bytes);
}

namespace GR_Main {
void Ex_OKGR_TransBuf_Draw_RGBA(void* d, std::int32_t p, void* s) {
  ::OKGR_TransBuf_Draw_RGBA(d, p, static_cast<const OkgfRleHeader*>(s));
}
void Ex_OKGR_AlphaBuf_Draw_RGBA(void* d, std::int32_t p, void* s) {
  ::OKGR_AlphaBuf_Draw_RGBA(d, p, static_cast<const OkgfRleHeader*>(s));
}
void Ex_OKGR_TransAlphaBuf_Draw_RGBA(void* d, std::int32_t p, void* s) {
  ::OKGR_TransAlphaBuf_Draw_RGBA(d, p, static_cast<const OkgfRleHeader*>(s));
}
void OKGF_Rescale(void* dest, std::int32_t w, std::int32_t h, std::int32_t pitch,
                  void* source, std::int32_t sw, std::int32_t sh,
                  std::int32_t sp, std::int32_t bpp, std::int32_t filter) {
  ::OKGF_Rescale(dest, w, h, pitch, source, sw, sh, sp, bpp, filter);
}
}

int main(int argc, char** argv) {
  constexpr int width = 1920, height = 1200, pitch = width * 4;
  okgf_set_math_precision(OKGF_MATH_GAME);
  std::vector<std::uint8_t> source(pitch * height);
  for (std::size_t i = 0; i < source.size(); ++i)
    source[i] = static_cast<std::uint8_t>((i * 37u + i / pitch) & 255u);
  std::vector<std::uint8_t> expected(1280 * 720 * 4);
  ::OKGF_Rescale(expected.data(), 1280, 720, 5120,
                 source.data() + 60 * pitch, 1920, 1080, pitch, 4, 5);
  if (std::all_of(expected.begin(), expected.end(), [](auto value) { return value == 0; }))
    return 4;

  for (int position = 0; position <= 5; ++position) {
    auto* graph = pas::construct_call<GR_GraphBuf::TGraphBufGR>(
        GR_GraphBuf::TGraphBufGR_Create, false);
    graph->AllocateRgbaTight(width, height);
    std::memcpy(graph->Pixels, source.data(), source.size());
    auto* original = graph->Pixels;
    allocations = failures = 0;
    fail_at = position;
    bool rejected = false;
    try { graph->RescaleRGBA_HW(1280, 720, true, 1, 1); }
    catch (...) { rejected = true; }
    fail_at = 0;
    if (position) {
      // Inspect original pixels only after confirming that they are still owned.
      if (!rejected || failures != 1 || graph->Pixels != original ||
          graph->Width != width || graph->Height != height ||
          graph->PitchBytes != pitch || graph->StorageKind != 0 ||
          std::memcmp(graph->Pixels, source.data(), source.size()) != 0) {
        pas::free(graph);
        std::fprintf(stderr, "failed allocation was published or source was changed: position=%d\n", position);
        return 1;
      }
      graph->RescaleRGBA_HW(1280, 720, true, 1, 1);
    } else if (rejected) {
      pas::free(graph);
      return 2;
    }
    const bool same = graph->Width == 1280 && graph->Height == 720 &&
        graph->PitchBytes == 5120 &&
        std::memcmp(graph->Pixels, expected.data(), expected.size()) == 0;
    graph->Clear();
    pas::free(graph);
    if (!same) return 3;
    std::printf("position=%d original source retained, successful output equivalent, lifecycle PASS\n", position);
  }

  std::vector<std::uint8_t> encoded;
  if (argc == 2) {
    std::ifstream file(argv[1], std::ios::binary);
    encoded.assign(std::istreambuf_iterator<char>{file}, {});
    if (encoded.size() < 96 || encoded.size() > 32u * 1024u * 1024u) return 5;
  } else {
    auto* graph = pas::construct_call<GR_GraphBuf::TGraphBufGR>(GR_GraphBuf::TGraphBufGR_Create, false);
    graph->AllocateRgbaTight(width, height);
    std::memcpy(graph->Pixels, source.data(), source.size());
    auto* image = pas::construct_call<GR_gi::TgiGR>(GR_gi::TgiGR_Create);
    image->CreateFromGraphBuf(graph, 1);
    encoded.assign(static_cast<const std::uint8_t*>(image->Data),
                   static_cast<const std::uint8_t*>(image->Data) + image->DataSize);
    pas::free(image);
    pas::free(graph);
  }
  std::vector<std::uint8_t> cache_expected;
  for (int position = 0; position <= 5; ++position) {
    auto* input = pas::construct_call<EC_Buf::TBufEC>(EC_Buf::TBufEC_Create);
    input->AddBytes(encoded.data(), static_cast<std::int32_t>(encoded.size()));
    auto* cache = pas::construct_call<EC_CacheGI::TCGiEC>(EC_CacheGI::TCGiEC_Create);
    cache->CacheKey = pas::WideString(u"Bm.FormRuins.2RCbg");
    auto* original = input->Data;
    fail_at = position;
    allocations = failures = 0;
    bool rejected = false;
    try { cache->LoadFromConfigBuffer(input, pas::WideString()); }
    catch (...) { rejected = true; }
    fail_at = 0;
    if (position) {
      if (!rejected || failures != 1 || input->Data != original ||
          input->DataSize != static_cast<std::int32_t>(encoded.size()) ||
          !cache->Image->IsEmpty() ||
          std::memcmp(input->Data, encoded.data(), encoded.size()) != 0) {
        pas::free(input); pas::free(cache); return 6;
      }
      cache->LoadFromConfigBuffer(input, pas::WideString());
    } else if (rejected) {
      pas::free(input); pas::free(cache); return 7;
    }
    auto* image = cache->Image;
    if (image->DataSize != 96 + 1280 * 720 * 2 ||
        cache->ResidentBytes != image->DataSize || image->Header->Format != 0 ||
        image->Header->Bounds.Right != 1280 || image->Header->Bounds.Bottom != 720) {
      pas::free(input); pas::free(cache); return 8;
    }
    // Clear/free the caller's GI first; CacheGI must retain its own pixels.
    input->Clear();
    pas::free(input);
    const auto* pixels = static_cast<const std::uint8_t*>(image->Data) + 96;
    const auto bytes = static_cast<std::size_t>(image->DataSize - 96);
    if (!position) cache_expected.assign(pixels, pixels + bytes);
    const bool same = cache_expected.size() == bytes &&
        std::memcmp(pixels, cache_expected.data(), bytes) == 0;
    pas::free(cache);
    if (!same) return 9;
    std::printf("cache position=%d input retained, retry equivalent, owned copy and cleanup PASS\n", position);
  }
}
