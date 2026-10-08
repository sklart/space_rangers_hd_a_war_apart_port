#include "aft_font.hpp"
#include "m23_aft_fixture.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

using srhd_awa::platform::aft_font::AftFont;
namespace {
void Reject(const std::vector<std::uint8_t>& bytes) {
  static unsigned case_number;
  ++case_number;
  AftFont font;
  std::string error;
  const bool loaded = font.Load(bytes.data(), bytes.size(), &error);
  if (loaded) std::fprintf(stderr, "corruption case %u unexpectedly loaded\n", case_number);
  assert(!loaded);
  assert(!error.empty());
  assert(font.glyphs().empty() && !font.Find(u'A'));
}
}
int main(int argc, char** argv) {
  if (argc > 1) {
    for (int i = 1; i < argc; ++i) {
      std::ifstream file(argv[i], std::ios::binary);
      if (!file) return 2;
      std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
      AftFont real;
      std::string error;
      if (!real.Load(bytes.data(), bytes.size(), &error)) {
        std::fprintf(stderr, "AFT parse failed for %s: %s\n", argv[i], error.c_str());
        return 1;
      }
      const auto hash = real.fingerprint();
      std::printf("%s|%zu|%zu|%d|%d|%d|%d|%d|%08x|%016llx\n", argv[i],
                  real.glyphs().size(), real.duplicate_count(), real.line_height(),
                  real.centering_height(), real.above_baseline(), real.below_baseline(),
                  real.max_glyph_advance(), hash.crc32,
                  static_cast<unsigned long long>(hash.fnv64));
    }
    return 0;
  }
  auto source = m23_test::AftFixture();
  AftFont font;
  std::string error;
  assert(!font.Find(u'A'));
  assert(font.Load(source.data(), source.size(), &error));
  assert(error.empty());
  assert(font.glyphs().size() == 6 && font.duplicate_count() == 0);
  assert(font.Find(u'A') && font.Find(u'B') && font.Find(u'C') && font.Find(u'Ё') && font.Find(u'я'));
  assert(!font.Find(u'Z') && font.Advance(u'Z') == 0);
  assert(font.line_height() == 7 && font.centering_height() == 3);
  assert(font.above_baseline() == 3 && font.below_baseline() == 1);
  assert(font.max_glyph_advance() == 6);
  auto damaged = source;
  damaged[0] = 'x'; Reject(damaged); // bad magic
  damaged = source; m23_test::Put32(&damaged, 4, 2); Reject(damaged); // version
  damaged = source; m23_test::Put32(&damaged, 8, 0xffffffffu); Reject(damaged); // negative count
  damaged = source; m23_test::Put32(&damaged, 8, 65537); Reject(damaged); // absurd count
  damaged.assign(source.begin(), source.begin() + 32 + 5 * 64); Reject(damaged); // truncated table
  damaged = source; m23_test::Put32(&damaged, 32 + 64, 0x10000); Reject(damaged);
  damaged = source; m23_test::Put32(&damaged, 32 + 64 + 16 + 16, 0xffffffffu); Reject(damaged); // offset
  damaged = source; m23_test::Put32(&damaged, 32 + 64 + 16 + 20, 0x7fffffff); Reject(damaged); // size
  damaged = source; damaged[font.Find(u'A')->opaque.data_offset] = 0xff; Reject(damaged); // stream size
  damaged = source; damaged[font.Find(u'A')->opaque.data_offset + 16] = 0x83; Reject(damaged); // row overflow
  damaged = source;
  m23_test::Put32(&damaged, 32 + 2 * 64 + 40 + 20, 18);
  m23_test::Put32(&damaged, font.Find(u'B')->alpha.data_offset, 2);
  Reject(damaged); // literal payload shortage
  damaged = source; m23_test::Put32(&damaged, font.Find(u'B')->alpha.data_offset + 8, 3); Reject(damaged); // height
  damaged = source; m23_test::Put32(&damaged, 32 + 64 + 16 + 8, 0x7fffffff); Reject(damaged); // dimensions
  damaged = source; m23_test::Put32(&damaged, 20, 0x7fffffff); Reject(damaged); // line height overflow
  damaged = source; m23_test::Put32(&damaged, 32 + 64 + 4, 0x7fffffff); Reject(damaged); // advance overflow
  // Upstream's lookup table stores the final duplicate index.
  damaged = source; m23_test::Put32(&damaged, 32 + 5 * 64, U'A');
  assert(font.Load(damaged.data(), damaged.size(), &error));
  assert(font.duplicate_count() == 1 && font.Find(u'A') == &font.glyphs()[5]);
  std::puts("M23 AFT synthetic parser PASS");
}
