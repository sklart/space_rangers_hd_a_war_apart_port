#include "gai_cpu.hpp"
#include "gi_format0_cpu.hpp"
#include "zlib_bridge.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace srhd_awa::platform::gai_cpu;
namespace srhd_awa::platform::zlib_bridge {
std::int32_t Uncompress(void*, std::int32_t, void*, std::int32_t) { return -1; }
std::int32_t UncompressZl02(void*, std::int32_t, void*, std::int32_t) { return -1; }
}
namespace srhd_awa::platform::gi_format0_cpu {
Status Decode(const void*, std::size_t, Metadata*, CpuImage*, std::string*) { return Status::InvalidHeader; }
}
namespace {
void W32(std::vector<std::uint8_t>* bytes, std::size_t at, std::int32_t value) {
  const auto u = static_cast<std::uint32_t>(value);
  for (unsigned shift = 0; shift < 32; shift += 8) (*bytes)[at + shift / 8] = static_cast<std::uint8_t>(u >> shift);
}
std::vector<std::uint8_t> Fixture() {
  std::vector<std::uint8_t> bytes(128); std::memcpy(bytes.data(), "gai\0", 4);
  W32(&bytes, 4, 1); W32(&bytes, 16, 2); W32(&bytes, 20, 2); W32(&bytes, 24, 3); W32(&bytes, 32, 72); W32(&bytes, 36, 56);
  W32(&bytes, 48, 0); W32(&bytes, 56, 0); W32(&bytes, 64, 0);
  W32(&bytes, 72, 2); W32(&bytes, 80, 24); W32(&bytes, 88, 44);
  W32(&bytes, 96, 2); W32(&bytes, 100, 0); W32(&bytes, 104, 20); W32(&bytes, 108, 1); W32(&bytes, 112, 40);
  W32(&bytes, 116, 1); W32(&bytes, 120, 2); W32(&bytes, 124, 60); return bytes;
}
}
int main() {
  auto bytes = Fixture(); GaiSequence first{}, second{};
  const bool ok = ReadGaiSequence(bytes.data(), bytes.size(), 0, &first) == Status::Ok &&
      ReadGaiSequence(bytes.data(), bytes.size(), 1, &second) == Status::Ok && first.frames.size() == 2 &&
      first.frames[0].source_frame_index == 0 && first.frames[0].delay_ms == 20 && first.frames[1].source_frame_index == 1 &&
      first.frames[1].delay_ms == 40 && second.frames.size() == 1 && second.frames[0].source_frame_index == 2 &&
      second.frames[0].delay_ms == 60;
  W32(&bytes, 100, -1);
  if (!ok || ReadGaiSequence(bytes.data(), bytes.size(), 0, &first) == Status::Ok) return 1;
  std::puts("GAI SEQUENCE CPU PASS");
}
