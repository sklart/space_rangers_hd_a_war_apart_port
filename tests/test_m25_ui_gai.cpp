#include "gai_frame_sequence_cpu.hpp"
#include "ui_gai.hpp"
#include "scene_compositor.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
using namespace srhd_awa::platform;
void Check(bool value, const char* reason) {
  if (!value) { std::fprintf(stderr, "M25 UI GAI FAIL: %s\n", reason); std::exit(1); }
}
void Put(std::vector<std::uint8_t>* bytes, std::size_t at, std::uint32_t value) {
  for (unsigned shift = 0; shift != 32; shift += 8)
    (*bytes)[at + shift / 8] = static_cast<std::uint8_t>(value >> shift);
}
std::vector<std::uint8_t> Gi(std::int32_t x, std::int32_t y, std::uint16_t color) {
  std::vector<std::uint8_t> bytes(104);
  std::memcpy(bytes.data(), "gi\0", 3);
  Put(&bytes, 4, 1); Put(&bytes, 8, x); Put(&bytes, 12, y);
  Put(&bytes, 16, x + 2); Put(&bytes, 20, y + 2);
  Put(&bytes, 24, 0xf800); Put(&bytes, 28, 0x07e0); Put(&bytes, 32, 0x001f);
  Put(&bytes, 44, 1); Put(&bytes, 64, 96); Put(&bytes, 68, 8);
  Put(&bytes, 80, x + 2); Put(&bytes, 84, y + 2);
  for (std::size_t at = 96; at != 104; at += 2) {
    bytes[at] = static_cast<std::uint8_t>(color);
    bytes[at + 1] = static_cast<std::uint8_t>(color >> 8);
  }
  return bytes;
}
std::vector<std::uint8_t> Gai() {
  const std::array<std::vector<std::uint8_t>, 3> frames{
      Gi(11, 20, 0xf800), Gi(10, 21, 0x07e0), Gi(12, 22, 0x001f)};
  constexpr std::size_t source = 72, table = source + 3 * 104;
  std::vector<std::uint8_t> bytes(table + 44);
  std::memcpy(bytes.data(), "gai\0", 4);
  Put(&bytes, 4, 1); Put(&bytes, 8, 10); Put(&bytes, 12, 20);
  Put(&bytes, 16, 14); Put(&bytes, 20, 24); Put(&bytes, 24, 3);
  Put(&bytes, 32, table); Put(&bytes, 36, 44);
  for (int index = 0; index != 3; ++index) {
    Put(&bytes, 48 + index * 8, source + index * 104);
    Put(&bytes, 52 + index * 8, 104);
    std::memcpy(bytes.data() + source + index * 104, frames[index].data(), 104);
  }
  Put(&bytes, table, 1); Put(&bytes, table + 8, 16); Put(&bytes, table + 16, 3);
  for (int index = 0; index != 3; ++index) {
    Put(&bytes, table + 20 + index * 8, index);
    Put(&bytes, table + 24 + index * 8, 10);
  }
  return bytes;
}
}

int main() {
  std::string error;
  gai_cpu::GaiSequence sequence{};
  Check(gai_frame_sequence_cpu::Parse("[5,2-0][10,1-2]", 3, &sequence, &error), error.c_str());
  Check(sequence.frames.size() == 5 && sequence.frames[0].source_frame_index == 2 &&
        sequence.frames[2].source_frame_index == 0 && sequence.frames[4].source_frame_index == 2,
        "ascending and descending ranges");
  for (const char* invalid : {"", "[0,0-1]", "[-1,0-1]", "[10,0-3]", "[10,0-]", "[10,0-1", "[10,0-2147483648]"})
    Check(!gai_frame_sequence_cpu::Parse(invalid, 3, &sequence, &error) && sequence.frames.empty(),
          "invalid custom sequence accepted");
  const auto source = Gai();
  ui::UiGaiLeaf leaf;
  leaf.SetSize({4, 4});
  Check(leaf.LoadBytes(source.data(), source.size(), "fixture.gai", &error), error.c_str());
  Check(leaf.ContentOriginX() == 10 && leaf.ContentOriginY() == 20 &&
        leaf.Animation().FrameOffsetX() == 1 && leaf.Animation().FrameOffsetY() == 0,
        "container and frame origin");
  std::array<std::uint16_t, 16> pixels{};
  scene_compositor::Framebuffer framebuffer{pixels.data(), 4, 4, 4};
  Check(leaf.Render(framebuffer, {0, 0, 4, 4}, &error) && pixels[1] == 0xf800 &&
        pixels[0] == 0 && leaf.HitTestPixel({1, 0}) && !leaf.HitTestPixel({0, 0}),
        "first frame placement and hit test");
  Check(leaf.Update(10, &error) && leaf.Animation().SourceFrame() == 1 &&
        leaf.Animation().FrameOffsetX() == 0 && leaf.Animation().FrameOffsetY() == 1,
        "embedded sequence advances with delta");
  pixels.fill(0);
  Check(leaf.Render(framebuffer, {0, 0, 4, 4}, &error) && pixels[4] == 0x07e0,
        "second frame origin and color");
  Check(leaf.SelectCustomSequence("[5,2-0]", &error) &&
        leaf.Animation().SourceFrame() == 2 && leaf.Animation().FrameOffsetX() == 2,
        "custom sequence selects first source frame");
  leaf.SetStopAfterOneCycle(true);
  Check(leaf.Update(15, &error) && !leaf.Active() && leaf.Animation().CyclesCompleted() == 1,
        "StopAfterOneCycle deactivates on wrap");
  ui::UiGaiLeaf stopped;
  Check(stopped.LoadBytes(source.data(), source.size(), "fixture.gai", &error) &&
        stopped.Stop(&error) && stopped.Update(100, &error) && stopped.Animation().SourceFrame() == 0,
        "Stop freezes playback");
  std::puts("M25 UI GAI PASS");
}
