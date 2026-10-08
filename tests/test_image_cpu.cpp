#include "image_cpu.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {
using srhd_awa::platform::image_cpu::Decode;
using srhd_awa::platform::image_cpu::Format;
using srhd_awa::platform::image_cpu::Image;

bool Expect(bool value, const char* what) {
  if (!value) std::fprintf(stderr, "FAIL: %s\n", what);
  return value;
}

void Put32(std::vector<std::uint8_t>* bytes, std::size_t offset, std::uint32_t value) {
  for (unsigned index = 0; index != 4; ++index) (*bytes)[offset + index] = value >> (index * 8);
}

std::vector<std::uint8_t> MakeBmp() {
  // 2x1, 24-bit BGR: red followed by green; BMP scanlines are four-byte aligned.
  std::vector<std::uint8_t> bytes(62, 0);
  bytes[0] = 'B'; bytes[1] = 'M'; Put32(&bytes, 2, static_cast<std::uint32_t>(bytes.size()));
  Put32(&bytes, 10, 54); Put32(&bytes, 14, 40); Put32(&bytes, 18, 2); Put32(&bytes, 22, 1);
  bytes[26] = 1; bytes[28] = 24; Put32(&bytes, 34, 8);
  bytes[54] = 0; bytes[55] = 0; bytes[56] = 255;
  bytes[57] = 0; bytes[58] = 255; bytes[59] = 0;
  return bytes;
}

bool TestFormats() {
  const auto bmp = MakeBmp();
  std::string error;
  Image image;
  if (!Expect(Decode(bmp.data(), static_cast<std::int32_t>(bmp.size()), Format::BGRA8888,
                     &image, &error), "BGRA decode")) return false;
  if (!Expect(image.width == 2 && image.height == 1 && image.pitch == 8 &&
              image.pixels[0] == 0 && image.pixels[1] == 0 && image.pixels[2] == 255 &&
              image.pixels[3] == 0 && image.pixels[4] == 0 && image.pixels[5] == 255 &&
              image.pixels[6] == 0 && image.pixels[7] == 0, "BGRA byte order")) return false;
  if (!Expect(Decode(bmp.data(), static_cast<std::int32_t>(bmp.size()), Format::RGB565,
                     &image, &error), "RGB565 decode") ||
      !Expect(image.pitch == 4 && image.pixels[0] == 0 && image.pixels[1] == 0xf8 &&
              image.pixels[2] == 0xe0 && image.pixels[3] == 0x07, "RGB565 pixels")) return false;
  if (!Expect(Decode(bmp.data(), static_cast<std::int32_t>(bmp.size()), Format::RGB888,
                     &image, &error), "RGB decode") ||
      !Expect(image.pitch == 6 && image.pixels[0] == 255 && image.pixels[1] == 0 &&
              image.pixels[2] == 0 && image.pixels[3] == 0 && image.pixels[4] == 255 &&
              image.pixels[5] == 0, "RGB byte order")) return false;
  if (!Expect(Decode(bmp.data(), static_cast<std::int32_t>(bmp.size()), Format::Gray8,
                     &image, &error), "gray decode") ||
      !Expect(image.pitch == 2 && image.pixels.size() == 2, "gray layout")) return false;
  return true;
}

bool TestFailuresAndReload() {
  const auto bmp = MakeBmp();
  std::string error;
  Image image;
  if (!Expect(Decode(bmp.data(), static_cast<std::int32_t>(bmp.size()), Format::RGB565,
                     &image, &error), "initial decode")) return false;
  const std::uint8_t truncated[] = {'B', 'M'};
  if (!Expect(!Decode(truncated, sizeof(truncated), Format::RGB565, &image, &error) &&
              image.width == 0 && image.height == 0 && image.pitch == 0 && image.empty(),
              "truncated clears destination")) return false;
  auto oversized = MakeBmp();
  Put32(&oversized, 18, 100000); Put32(&oversized, 22, 100000);
  if (!Expect(!Decode(oversized.data(), static_cast<std::int32_t>(oversized.size()),
                     Format::BGRA8888, &image, &error) && image.empty(), "oversize rejected")) return false;
  return Expect(Decode(bmp.data(), static_cast<std::int32_t>(bmp.size()), Format::RGB565,
                       &image, &error) && image.width == 2, "reload after failure");
}

}  // namespace

int main() {
  if (!TestFormats() || !TestFailuresAndReload()) return 1;
  std::puts("IMAGE CPU PASS");
  return 0;
}
