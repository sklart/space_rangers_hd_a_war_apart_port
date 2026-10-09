#include "graph_buffer_cpu.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include <zlib.h>

namespace {
using namespace srhd_awa::platform;
using graph_buffer_cpu::GraphBufferCpu;
void Check(bool value, const char* reason) {
  if (!value) { std::fprintf(stderr, "M26 GRAPHBUF FAIL: %s\n", reason); std::exit(1); }
}
void Put(std::vector<std::uint8_t>* bytes, std::size_t at, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    (*bytes)[at + shift / 8] = static_cast<std::uint8_t>(value >> shift);
}
std::uint64_t Fnv(const std::vector<std::uint8_t>& bytes) {
  std::uint64_t value = UINT64_C(0xcbf29ce484222325);
  for (const auto byte : bytes) value = (value ^ byte) * UINT64_C(0x100000001b3);
  return value;
}
std::uint32_t Crc(const std::vector<std::uint8_t>& bytes) {
  return static_cast<std::uint32_t>(crc32(0, bytes.data(), bytes.size()));
}
std::vector<std::uint8_t> Gi3x3() {
  std::vector<std::uint8_t> bytes(96 + 36, 0);
  std::memcpy(bytes.data(), "gi\0", 3);
  Put(&bytes, 4, 1); Put(&bytes, 8, 0); Put(&bytes, 12, 0);
  Put(&bytes, 16, 3); Put(&bytes, 20, 3);
  Put(&bytes, 36, 0xff000000); Put(&bytes, 44, 1);
  Put(&bytes, 64, 96); Put(&bytes, 68, 36);
  Put(&bytes, 72, 0); Put(&bytes, 76, 0); Put(&bytes, 80, 3); Put(&bytes, 84, 3);
  for (std::size_t at = 96; at < bytes.size(); at += 4) {
    bytes[at + 2] = 255;
    bytes[at + 3] = 255;
  }
  return bytes;
}
}

int main() {
  std::string error;
  image_layout::Point fit{};
  Check(GraphBufferCpu::AspectFit(4, 2, 6, 6, &fit, &error) && fit.x == 6 && fit.y == 3,
        "horizontal aspect fit");
  Check(GraphBufferCpu::AspectFit(2, 4, 6, 6, &fit, &error) && fit.x == 3 && fit.y == 6,
        "vertical aspect fit");
  Check(!GraphBufferCpu::AspectFit(0, 2, 6, 6, &fit, &error) && !error.empty(),
        "zero source width rejected");

  GraphBufferCpu buffer;
  Check(!buffer.AllocateBuffer(0, 2, &error) && !buffer.owns_buffer(), "zero allocation rejected");
  Check(!buffer.AllocateBuffer(std::numeric_limits<std::int32_t>::max(), 2, &error),
        "overflow allocation rejected");
  Check(buffer.AllocateBuffer(2, 2, &error) && buffer.owns_buffer() &&
        buffer.pixel_format() == graph_buffer_cpu::PixelFormat::RGB565 &&
        buffer.pitch() == 4 && buffer.bytes() == 8 &&
        !buffer.source_has_per_pixel_alpha(), "owned RGB565 allocation");
  Check(buffer.SetPixel565(0, 0, 0xf800) && !buffer.SetPixel565(2, 0, 0),
        "bounded native pixel access");
  std::array<std::uint16_t, 16> pixels{};
  scene_compositor::Framebuffer target{pixels.data(), 4, 4, 4};
  const okgf_rle_bridge::Rect area{0, 0, 4, 4};
  Check(buffer.Draw(target, area, area, image_layout::XMode::LeftFill,
                    image_layout::YMode::TopFill, false, &error), error.c_str());
  Check(pixels[0] == 0xf800 && pixels[2] == 0xf800 && pixels[5] == 0,
        "RGB565 tiled draw");
  Check(buffer.HitTestPixel(2, 0, area, area, image_layout::XMode::LeftFill,
                            image_layout::YMode::TopFill, false) &&
        !buffer.HitTestPixel(1, 0, area, area, image_layout::XMode::LeftFill,
                             image_layout::YMode::TopFill, false), "black-pixel hit");
  const auto center = buffer.GetVisualCenter(area, area,
      image_layout::XMode::LeftFill, image_layout::YMode::TopFill, false);
  Check(center.x == 1 && center.y == 1, "local visual center of tiled pixels");
  Check(!buffer.Draw(target, {0, 0, 16384, 16384}, area,
                     image_layout::XMode::LeftFill, image_layout::YMode::TopFill,
                     false, &error), "pathological tile count rejected");
  Check(!buffer.Draw(target, {std::numeric_limits<std::int32_t>::max() - 1, 0,
                              std::numeric_limits<std::int32_t>::max(), 2}, area,
                     image_layout::XMode::Right, image_layout::YMode::Top,
                     false, &error), "extreme layout coordinates rejected");
  pixels.fill(0x001f);
  Check(buffer.Draw(target, {0, 0, 2, 2}, {0, 0, 2, 2}, image_layout::XMode::Center,
                    image_layout::YMode::Center, true, &error) && pixels[0] == 0x780f,
        "RGB565 HalfAlpha matches half copy");
  buffer.ClearOwnedBuffer();
  Check(!buffer.owns_buffer() && buffer.bytes() == 0, "clear owned buffer");

  constexpr std::uint8_t bgra[] = {
      0, 0, 255, 255, 0, 255, 0, 255,
      255, 0, 0, 255, 0, 0, 0, 0};
  Check(buffer.LoadBgraPixels(bgra, 2, 2, 8, &error) &&
        buffer.ScaleAspectFit(4, 4, graph_buffer_cpu::ScaleFilter::BilinearRGBA, &error) &&
        buffer.width() == 4 && buffer.height() == 4 && buffer.pitch() == 16 &&
        buffer.source_has_per_pixel_alpha(), "bilinear BGRA scaling");
  Check(Crc(buffer.pixels()) == 0x8b93ffa1u &&
        Fnv(buffer.pixels()) == UINT64_C(0x2823819d585c1ce9),
        "independent Python bilinear BGRA oracle");
  pixels.fill(0);
  Check(buffer.Draw(target, area, area, image_layout::XMode::Center,
                    image_layout::YMode::Center, true, &error) && pixels[0] == 0xf800,
        "BGRA alpha draw ignores HalfAlpha");
  Check(buffer.HitTestPixel(3, 3, area, area, image_layout::XMode::Center,
                            image_layout::YMode::Center, false),
        "bilinear interpolation preserves fractional alpha");

  auto gi = Gi3x3();
  Check(buffer.LoadGiBytes(gi.data(), gi.size(), 6, 6, &error) &&
        buffer.width() == 6 && buffer.height() == 6 &&
        buffer.pixel_format() == graph_buffer_cpu::PixelFormat::BGRA8888 &&
        buffer.bytes() == 144, "GI uses M25 decode and OKGF filter 5");
  std::printf("M26 GRAPHBUF GI scaled=%08x/%016llx\n", Crc(buffer.pixels()),
              static_cast<unsigned long long>(Fnv(buffer.pixels())));
  Check(Crc(buffer.pixels()) == 0x700a059cu &&
        Fnv(buffer.pixels()) == UINT64_C(0xec32669a74fcae65),
        "OKGF Lanczos3 synthetic regression fingerprint");
  gi.resize(20);
  Check(!buffer.LoadGiBytes(gi.data(), gi.size(), 6, 6, &error),
        "corrupt GI source rejected");
  Check(buffer.width() == 6 && buffer.height() == 6 && buffer.bytes() == 144,
        "failed load preserves last complete buffer");
  std::puts("M26 GRAPHBUF CPU PASS");
}
