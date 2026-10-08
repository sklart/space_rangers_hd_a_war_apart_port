#include "package.hpp"
#include "scene_compositor.hpp"
#include "ui_gai.hpp"
#include "ui_tree_fingerprint.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
using namespace srhd_awa::platform;
struct Hash { std::uint32_t crc{}; std::uint64_t fnv{}; };
Hash Fingerprint(const std::uint8_t* bytes, std::size_t size) {
  Hash value{0xffffffffu, 0xcbf29ce484222325ull};
  for (std::size_t i{}; i != size; ++i) {
    value.crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit)
      value.crc = (value.crc >> 1) ^ ((value.crc & 1) ? 0xedb88320u : 0u);
    value.fnv = (value.fnv ^ bytes[i]) * 0x100000001b3ull;
  }
  value.crc ^= 0xffffffffu;
  return value;
}
void Check(bool condition, const std::string& reason) {
  if (!condition) { std::fprintf(stderr, "M25 GAI RELEASE FAIL: %s\n", reason.c_str()); std::exit(1); }
}
}

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::string error;
  srhd_awa::package::Package package;
  Check(package.Open(std::string(argv[1]) + "/DATA/common.pkg", &error), error);
  constexpr const char* resource = "DATA/PI/PathEndMove.gai";
  const auto* entry = package.Resolve(resource);
  Check(entry != nullptr, "GAI release resource missing");
  std::vector<std::uint8_t> source;
  Check(package.ReadPayload(*entry, &source, &error), error);
  const auto source_hash = Fingerprint(source.data(), source.size());
  Check(source.size() == 27402 && source_hash.crc == 0x3bf46ce9u &&
        source_hash.fnv == 0x7b4a7f853b191bc5ull, "Python GAI source oracle");
  ui::UiGaiLeaf leaf(&package);
  Check(leaf.LoadResource(resource, &error) && leaf.SelectEmbeddedSequence(0, &error), error);
  Check(leaf.ClientSize() == ui::Size{32, 32} && leaf.Animation().Metadata().frame_count == 20 &&
        leaf.Animation().FrameOffsetX() == 4 && leaf.Animation().FrameOffsetY() == 6,
        "GAI canvas and first frame origin");
  std::array<std::uint16_t, 32 * 32> pixels{};
  scene_compositor::Framebuffer framebuffer{pixels.data(), 32, 32, 32};
  const auto check_frame = [&](std::uint32_t pixel_crc, std::uint64_t pixel_fnv,
                               std::uint32_t frame_crc, std::uint64_t frame_fnv) {
    const auto& frame = leaf.Animation().Image();
    const auto pixel_hash = Fingerprint(frame.pixels.data(), frame.pixels.size());
    Check(pixel_hash.crc == pixel_crc && pixel_hash.fnv == pixel_fnv,
          "Python GAI decoded frame oracle");
    pixels.fill(0);
    Check(leaf.Render(framebuffer, {0, 0, 32, 32}, &error), error);
    ui_fingerprint::Value rendered{};
    Check(ui_fingerprint::ComputeFramebuffer(framebuffer, &rendered, &error), error);
    Check(rendered.crc32 == frame_crc && rendered.fnv64 == frame_fnv,
          "Python GAI RGB565 canvas oracle");
  };
  check_frame(0x6712c20cu, 0x29d67a095d932591ull,
              0x71b457cdu, 0xd75029d766f2bdedull);
  Check(leaf.Update(69, &error) && leaf.Animation().SourceFrame() == 0,
        "GAI 69ms before first 70ms boundary");
  Check(leaf.Update(1, &error) && leaf.Animation().SourceFrame() == 1,
        "GAI advances at 70ms boundary");
  check_frame(0xf47481fau, 0xe56766d46180bfc9ull,
              0xed8aac30u, 0x4b7e19dd0fbc527aull);
  std::puts("M25 GAI RELEASE PASS");
}
