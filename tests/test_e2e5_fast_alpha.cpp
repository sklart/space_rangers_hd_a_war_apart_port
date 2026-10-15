#include "okgf_fast_alpha.hpp"
#include "okgf.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

std::uint32_t Next(std::uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

bool Compare(std::int32_t width, std::int32_t height, bool sparse) {
  constexpr std::int32_t kExtra = 7;
  const std::int32_t source_pitch = (width + kExtra) * 4;
  const std::int32_t dest_pitch = (width + kExtra) * 2;
  std::vector<std::uint8_t> source(static_cast<std::size_t>(source_pitch) * (height + kExtra));
  std::vector<std::uint8_t> reference(static_cast<std::size_t>(dest_pitch) * (height + kExtra));
  std::uint32_t seed = 0x6953a16fu;
  for (auto& byte : source) byte = static_cast<std::uint8_t>(Next(seed));
  for (auto& byte : reference) byte = static_cast<std::uint8_t>(Next(seed));
  for (std::int32_t y = 0; y < height; ++y) {
    for (std::int32_t x = 0; x < width; ++x) {
      auto& alpha = source[static_cast<std::size_t>(y + 2) * source_pitch +
                           static_cast<std::size_t>(x + 3) * 4 + 3];
      if (height == 1 && width == 256) alpha = static_cast<std::uint8_t>(x);
      else if (sparse) {
        const unsigned selector = Next(seed) % 16;
        alpha = selector < 11 ? 0 : selector < 14 ? 255 : alpha;
      }
    }
  }
  auto optimized = reference;
  const auto initial = reference;
  OKGR_AlphaSimpleBuf_Draw_16(reference.data(), dest_pitch, 1, 3,
                              source.data(), source_pitch, 3, 2, width, height);
  srhd_awa::platform::okgf_fast_alpha::AlphaSimpleBufDraw16(
      optimized.data(), dest_pitch, 1, 3, source.data(), source_pitch,
      3, 2, width, height);
  if (optimized != reference) {
    std::fputs("first alpha pass differs from OKGF\n", stderr);
    return false;
  }
  reference = initial;
  optimized = initial;
  constexpr std::int32_t kIterations = 4;
  const auto began_reference = std::chrono::steady_clock::now();
  for (std::int32_t i = 0; i < kIterations; ++i)
    OKGR_AlphaSimpleBuf_Draw_16(reference.data(), dest_pitch, 1, 3,
                                source.data(), source_pitch, 3, 2, width, height);
  const auto ended_reference = std::chrono::steady_clock::now();
  for (std::int32_t i = 0; i < kIterations; ++i)
    srhd_awa::platform::okgf_fast_alpha::AlphaSimpleBufDraw16(
        optimized.data(), dest_pitch, 1, 3, source.data(), source_pitch,
        3, 2, width, height);
  const auto ended_optimized = std::chrono::steady_clock::now();
  if (optimized != reference) {
    const auto at = std::mismatch(optimized.begin(), optimized.end(), reference.begin()).first -
                    optimized.begin();
    std::fprintf(stderr, "alpha mismatch width=%d height=%d sparse=%d byte=%td got=%u expected=%u\n",
                 width, height, sparse, at, optimized[at], reference[at]);
    return false;
  }
  const auto reference_us = std::chrono::duration_cast<std::chrono::microseconds>(
      ended_reference - began_reference).count();
  const auto optimized_us = std::chrono::duration_cast<std::chrono::microseconds>(
      ended_optimized - ended_reference).count();
  std::printf("alpha %dx%d sparse=%d reference_us=%lld optimized_us=%lld\n",
              width, height, sparse, static_cast<long long>(reference_us),
              static_cast<long long>(optimized_us));
  return true;
}

}  // namespace

int main() {
  if (!Compare(256, 1, false) || !Compare(17, 13, false) ||
      !Compare(700, 700, false) || !Compare(700, 700, true))
    return 1;
  std::puts("fast alpha: PASS");
  return 0;
}
