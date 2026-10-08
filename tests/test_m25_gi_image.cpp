#include "gi_image_cpu.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
using namespace srhd_awa::platform;
void Check(bool value, const char* reason) {
  if (!value) { std::fprintf(stderr, "M25 GI IMAGE FAIL: %s\n", reason); std::exit(1); }
}
void Put(std::vector<std::uint8_t>* bytes, std::size_t at, std::uint32_t value) {
  for (unsigned shift = 0; shift != 32; shift += 8)
    (*bytes)[at + shift / 8] = static_cast<std::uint8_t>(value >> shift);
}
std::vector<std::uint8_t> Format0() {
  std::vector<std::uint8_t> bytes(112);
  std::memcpy(bytes.data(), "gi\0", 3);
  Put(&bytes, 4, 1); Put(&bytes, 8, 7); Put(&bytes, 12, 9);
  Put(&bytes, 16, 9); Put(&bytes, 20, 11);
  Put(&bytes, 36, 0xff000000); Put(&bytes, 44, 1);
  Put(&bytes, 64, 96); Put(&bytes, 68, 16);
  Put(&bytes, 72, 7); Put(&bytes, 76, 9); Put(&bytes, 80, 9); Put(&bytes, 84, 11);
  const std::uint8_t pixels[] = {0, 0, 0, 255, 0, 0, 255, 255,
                                 0, 0, 0, 0, 0, 255, 0, 128};
  std::memcpy(bytes.data() + 96, pixels, sizeof pixels);
  return bytes;
}
void Plane(std::vector<std::uint8_t>* bytes, int index, int offset,
           const std::vector<std::uint8_t>& stream) {
  const auto at = 64 + index * 32;
  Put(bytes, at, offset); Put(bytes, at + 4, 16 + static_cast<std::uint32_t>(stream.size()));
  Put(bytes, at + 8, 7); Put(bytes, at + 12, 9);
  Put(bytes, at + 16, 11); Put(bytes, at + 20, 12);
  Put(bytes, offset, static_cast<std::uint32_t>(stream.size()));
  Put(bytes, offset + 4, 4); Put(bytes, offset + 8, 3);
  std::memcpy(bytes->data() + offset + 16, stream.data(), stream.size());
}
std::vector<std::uint8_t> Format2() {
  std::vector<std::uint8_t> bytes(320);
  std::memcpy(bytes.data(), "gi", 2);
  Put(&bytes, 4, 1); Put(&bytes, 8, 7); Put(&bytes, 12, 9);
  Put(&bytes, 16, 11); Put(&bytes, 20, 12);
  Put(&bytes, 40, 2); Put(&bytes, 44, 3);
  Plane(&bytes, 2, 160, {0x84,0,63,32,16,0,0x80,0x02,0x82,8,48,0});
  Plane(&bytes, 1, 192, {0x82,0,0xf8,0xe0,0x07,0x02,0,0x80,0x84,0x1f,0,0,0xf8,0xe0,0x07,0xff,0xff,0});
  Plane(&bytes, 0, 232, {0x01,0x83,0,0xf8,0xe0,0x07,0xff,0xff,0,0x80,0x84,0x1f,0,0,0xf8,0xe0,0x07,0xff,0xff,0});
  return bytes;
}
bool Reject(std::vector<std::uint8_t> bytes) {
  gi_image_cpu::GiImage image;
  std::string error;
  return !image.LoadBytes(bytes.data(), bytes.size(), &error) && !image.loaded() &&
         image.image().pixels.empty() && !error.empty();
}
}

int main() {
  auto bytes = Format0();
  gi_image_cpu::GiImage image;
  std::string error;
  Check(image.LoadBytes(bytes.data(), bytes.size(), &error), error.c_str());
  Check(image.format() == 0 && image.origin_x() == 7 && image.origin_y() == 9 &&
        image.width() == 2 && image.height() == 2, "Format0 natural bounds");
  const okgf_rle_bridge::Rect bounds{0, 0, 4, 4};
  Check(image.HitTestPixel(1, 0, bounds, bounds, image_layout::XMode::LeftFill,
                           image_layout::YMode::TopFill), "red pixel hit");
  Check(!image.HitTestPixel(0, 0, bounds, bounds, image_layout::XMode::LeftFill,
                            image_layout::YMode::TopFill), "black pixel does not hit");
  Check(image.HitTestPixel(3, 3, bounds, bounds, image_layout::XMode::LeftFill,
                           image_layout::YMode::TopFill), "partial alpha tiled hit");
  Check(!image.HitTestPixel(0, 0, bounds, {1, 0, 4, 4}, image_layout::XMode::LeftFill,
                            image_layout::YMode::TopFill), "clip excludes pixel");
  const auto center = image.GetVisualCenter({0, 0, 2, 2}, {0, 0, 2, 2},
                                            image_layout::XMode::Center,
                                            image_layout::YMode::Center);
  Check(center.x == 1 && center.y == 0, "local visual center");
  std::array<std::uint16_t, 16> framebuffer{};
  Check(image.Draw(framebuffer.data(), 4, 4, 4, bounds, bounds,
                   image_layout::XMode::LeftFill, image_layout::YMode::TopFill, 255, &error),
        error.c_str());
  Check(framebuffer[1] == 0xf800 && framebuffer[3] == 0xf800 &&
        framebuffer[0] == 0 && framebuffer[9] == 0xf800,
        "opaque and tiled RGB565 pixels");
  auto bad = bytes; bad.resize(63); Check(Reject(bad), "truncated header");
  bad = bytes; Put(&bad, 40, 7); Check(Reject(bad), "unsupported format");
  bad = bytes; Put(&bad, 16, 7); Check(Reject(bad), "zero width");
  bad = bytes; Put(&bad, 16, 0x7fffffff); Put(&bad, 20, 0x7fffffff);
  Check(Reject(bad), "oversized decoded bounds");
  bad = bytes; Put(&bad, 64, 0x7fffffff); Check(Reject(bad), "plane offset overflow");
  bad = bytes; Put(&bad, 68, 0x7fffffff); Check(Reject(bad), "plane size overflow");
  bad = bytes; Put(&bad, 44, 100); Check(Reject(bad), "plane table overflow");
  auto format2 = Format2();
  Check(image.LoadBytes(format2.data(), format2.size(), &error) && image.format() == 2 &&
        image.origin_x() == 7 && image.origin_y() == 9 && image.width() == 4 &&
        image.height() == 3 && image.image().pixels[3] == 252,
        "Format2 nonzero origin and partial alpha");
  bad = format2; Put(&bad, 160, 2); bad[176] = 0xff;
  Check(Reject(bad), "Format2 malformed RLE");
  std::puts("M25 GI IMAGE PASS");
}
