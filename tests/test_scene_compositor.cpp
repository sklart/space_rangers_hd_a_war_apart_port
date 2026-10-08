#include "scene_compositor.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {
using namespace srhd_awa::platform::scene_compositor;

bool Expect(bool value, const char* label) {
  if (!value) std::fprintf(stderr, "FAIL: %s\n", label);
  return value;
}

std::uint32_t Crc32(const std::uint8_t* bytes, std::size_t size) {
  std::uint32_t crc = 0xffffffffu;
  for (std::size_t index = 0; index < size; ++index) {
    crc ^= bytes[index];
    for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}
std::uint64_t Fnv64(const std::uint8_t* bytes, std::size_t size) {
  std::uint64_t value = UINT64_C(14695981039346656037);
  for (std::size_t index = 0; index < size; ++index) value = (value ^ bytes[index]) * UINT64_C(1099511628211);
  return value;
}

srhd_awa::platform::gi_format0_cpu::CpuImage Image(std::uint8_t blue, std::uint8_t green, std::uint8_t red, std::uint8_t alpha = 255) {
  srhd_awa::platform::gi_format0_cpu::CpuImage image;
  image.width = image.height = 1; image.bytes_per_pixel = 4; image.pitch = 4;
  image.pixels = {blue, green, red, alpha};
  return image;
}

bool Add(Scene* scene, const char* id, std::uint8_t b, std::uint8_t g, std::uint8_t r,
         std::int32_t x, std::int32_t y, std::int32_t layer, std::uint8_t alpha = 255, bool visible = true) {
  SceneSprite sprite{id, Image(b, g, r), x, y, alpha, layer, visible};
  std::string error;
  return scene->AddSprite(std::move(sprite), &error);
}

bool TestLayerOrder() {
  Scene scene;
  std::vector<std::uint16_t> pixels(1, 0);
  const Framebuffer target{pixels.data(), 1, 1, 1};
  std::string error;
  return Expect(Add(&scene, "background", 255, 0, 0, 0, 0, 0), "background add") &&
         Expect(Add(&scene, "object", 0, 255, 0, 0, 0, 10), "object add") &&
         Expect(Add(&scene, "foreground", 0, 0, 255, 0, 0, 20), "foreground add") &&
         Expect(scene.Render(target, &error), "layer render") &&
         Expect(pixels[0] == 0xf800, "foreground wins") &&
         Expect(!Add(&scene, "foreground", 0, 0, 0, 0, 0, 30), "duplicate rejected") &&
         Expect(scene.RemoveSprite("object") && !scene.RemoveSprite("none"), "remove sprite");
}

bool TestAlphaAndFingerprint() {
  Scene scene;
  std::vector<std::uint16_t> pixels(4, 0);
  const Framebuffer target{pixels.data(), 2, 2, 2};
  std::string error;
  if (!Add(&scene, "blue", 255, 0, 0, 0, 0, 0) || !Add(&scene, "red50", 0, 0, 255, 0, 0, 10, 128) ||
      !Add(&scene, "green50", 0, 255, 0, 0, 0, 20, 128) || !Expect(scene.Render(target, &error), "alpha render")) return false;
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(pixels.data());
  Fingerprint fingerprint{};
  return Expect(pixels[0] == 0x4407, "alpha exact RGB565") &&
         Expect(Crc32(bytes, pixels.size() * sizeof(std::uint16_t)) == 0xdfa3871au, "alpha CRC32") &&
         Expect(Fnv64(bytes, pixels.size() * sizeof(std::uint16_t)) == UINT64_C(0xe1eed2594b085936), "alpha FNV64") &&
         Expect(scene.ComputeFingerprint(&fingerprint, &error), "scene fingerprint") &&
         Expect(fingerprint.crc32 == 0x8faebfebu && fingerprint.fnv64 == UINT64_C(0x5478fac00abb65d2), "scene fingerprint exact");
}

bool TestPositionVisibilityAndDeterminism() {
  Scene scene;
  std::vector<std::uint16_t> first(16, 0x001f), second(16, 0x001f);
  const Framebuffer target{first.data(), 4, 4, 4};
  std::string error;
  if (!Add(&scene, "origin", 0, 0, 255, 0, 0, 0) || !Add(&scene, "centre", 0, 255, 0, 2, 2, 1) ||
      !Add(&scene, "negative", 255, 0, 0, -3, -3, 2) || !Add(&scene, "outside", 255, 255, 255, 99, 99, 3) ||
      !Add(&scene, "hidden", 255, 255, 255, 1, 1, 4, 255, false) || !Expect(scene.Render(target, &error), "position render")) return false;
  const bool positions = first[0] == 0xf800 && first[10] == 0x07e0 && first[5] == 0x001f;
  second.assign(16, 0x001f);
  const Framebuffer again{second.data(), 4, 4, 4};
  return Expect(positions, "origin centre hidden") && Expect(scene.Render(again, &error), "repeat render") &&
         Expect(first == second, "deterministic pixels") &&
         Expect(Crc32(reinterpret_cast<const std::uint8_t*>(first.data()), first.size() * 2) ==
                    Crc32(reinterpret_cast<const std::uint8_t*>(second.data()), second.size() * 2), "deterministic CRC") &&
         Expect(Fnv64(reinterpret_cast<const std::uint8_t*>(first.data()), first.size() * 2) ==
                    Fnv64(reinterpret_cast<const std::uint8_t*>(second.data()), second.size() * 2), "deterministic FNV");
}
}  // namespace

int main() {
  if (!TestLayerOrder() || !TestAlphaAndFingerprint() || !TestPositionVisibilityAndDeterminism()) return 1;
  std::puts("SCENE COMPOSITOR PASS");
  return 0;
}
