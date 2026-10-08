#include "software_compositor.hpp"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace {

using srhd_awa::platform::software_compositor::BlendMode;
using srhd_awa::platform::software_compositor::CompositeBGRA;
using srhd_awa::platform::software_compositor::Rect;

bool Expect(bool value, const char* what) {
  if (!value) std::fprintf(stderr, "FAIL: %s\n", what);
  return value;
}

std::uint32_t Crc32(const std::uint8_t* bytes, std::size_t count) {
  std::uint32_t crc = 0xffffffffu;
  for (std::size_t index = 0; index < count; ++index) {
    crc ^= bytes[index];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

std::uint64_t Fnv64(const std::uint8_t* bytes, std::size_t count) {
  std::uint64_t value = UINT64_C(14695981039346656037);
  for (std::size_t index = 0; index < count; ++index)
    value = (value ^ bytes[index]) * UINT64_C(1099511628211);
  return value;
}

void SetBgra(std::vector<std::uint8_t>* pixels, std::int32_t x, std::int32_t y,
             std::int32_t pitch, std::uint8_t blue, std::uint8_t green,
             std::uint8_t red, std::uint8_t alpha) {
  auto* value = pixels->data() + y * pitch + x * 4;
  value[0] = blue;
  value[1] = green;
  value[2] = red;
  value[3] = alpha;
}

bool TestOpaque() {
  std::vector<std::uint8_t> source(16);
  SetBgra(&source, 0, 0, 8, 0, 0, 255, 255);
  SetBgra(&source, 1, 0, 8, 0, 255, 0, 255);
  SetBgra(&source, 0, 1, 8, 255, 0, 0, 255);
  SetBgra(&source, 1, 1, 8, 255, 255, 255, 255);
  std::vector<std::uint16_t> destination(16, 0);
  std::string error;
  if (!Expect(CompositeBGRA(destination.data(), 4, 4, 4, source.data(), 2, 2,
                            8, 1, 1, BlendMode::Opaque, nullptr, &error),
              "opaque composite"))
    return false;
  const bool pixels = destination[5] == 0xf800 && destination[6] == 0x07e0 &&
                      destination[9] == 0x001f && destination[10] == 0xffff;
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(destination.data());
  return Expect(pixels, "opaque RGB565 pixels") &&
         Expect(Crc32(bytes, destination.size() * sizeof(std::uint16_t)) ==
                    0x3449ea92u,
                "opaque CRC32") &&
         Expect(Fnv64(bytes, destination.size() * sizeof(std::uint16_t)) ==
                    UINT64_C(0x28d6a056a7a3548d),
                "opaque FNV64");
}

bool TestAlpha() {
  std::vector<std::uint8_t> source(4, 0);
  SetBgra(&source, 0, 0, 4, 0, 0, 255, 128);
  std::uint16_t destination = 0x001f;
  std::string error;
  return Expect(CompositeBGRA(&destination, 1, 1, 1, source.data(), 1, 1, 4,
                              0, 0, BlendMode::Alpha, nullptr, &error),
                "alpha composite") &&
         Expect(destination == 0x800f, "50 percent alpha RGB565");
}

bool TestClippingAndRepeat() {
  std::vector<std::uint8_t> source(10 * 10 * 4);
  for (std::int32_t y = 0; y < 10; ++y)
    for (std::int32_t x = 0; x < 10; ++x)
      SetBgra(&source, x, y, 40, 0, 0, 255, 255);
  std::vector<std::uint16_t> destination(36, 0x001f);
  std::string error;
  const Rect clip{1, 1, 4, 4};
  if (!Expect(CompositeBGRA(destination.data(), 6, 6, 6, source.data(), 10, 10,
                            40, -5, -5, BlendMode::Opaque, &clip, &error),
              "negative position clipping"))
    return false;
  for (std::int32_t y = 0; y < 6; ++y)
    for (std::int32_t x = 0; x < 6; ++x) {
      const bool inside = x >= 1 && x < 4 && y >= 1 && y < 4;
      if (!Expect(destination[y * 6 + x] == (inside ? 0xf800 : 0x001f),
                  "clip visibility"))
        return false;
    }
  const auto first = destination;
  std::fill(destination.begin(), destination.end(), 0x001f);
  if (!Expect(CompositeBGRA(destination.data(), 6, 6, 6, source.data(), 10, 10,
                            40, -5, -5, BlendMode::Opaque, &clip, &error),
              "repeat composite"))
    return false;
  return Expect(destination == first, "repeat deterministic output");
}

bool TestInvalid() {
  std::uint16_t destination{};
  std::uint8_t source[4]{};
  std::string error;
  const Rect bad_clip{2, 1, 1, 2};
  return Expect(!CompositeBGRA(nullptr, 1, 1, 1, source, 1, 1, 4, 0, 0,
                               BlendMode::Opaque, nullptr, &error),
                "null destination") &&
         Expect(!CompositeBGRA(&destination, 1, 1, 1, nullptr, 1, 1, 4, 0,
                               0, BlendMode::Opaque, nullptr, &error),
                "null source") &&
         Expect(!CompositeBGRA(&destination, -1, 1, 1, source, 1, 1, 4, 0,
                               0, BlendMode::Opaque, nullptr, &error),
                "negative destination width") &&
         Expect(!CompositeBGRA(&destination, 1, 1, 1, source, 1, 1, 0, 0, 0,
                               BlendMode::Opaque, nullptr, &error),
                "zero source pitch") &&
         Expect(!CompositeBGRA(&destination, 1, 1, 1, source,
                               std::numeric_limits<std::int32_t>::max(), 1,
                               std::numeric_limits<std::int32_t>::max(), 0, 0,
                               BlendMode::Opaque, nullptr, &error),
                "oversized source") &&
         Expect(!CompositeBGRA(&destination, 1, 1, 1, source, 1, 1, 4, 0, 0,
                               BlendMode::Opaque, &bad_clip, &error),
                "invalid clip") &&
         Expect(CompositeBGRA(&destination, 1, 1, 1, source, 1, 1, 4, 5, 5,
                              BlendMode::Opaque, nullptr, &error),
                "outside clip is no-op");
}

}  // namespace

int main() {
  if (!TestOpaque() || !TestAlpha() || !TestClippingAndRepeat() || !TestInvalid())
    return 1;
  std::puts("SOFTWARE COMPOSITOR PASS");
  return 0;
}
