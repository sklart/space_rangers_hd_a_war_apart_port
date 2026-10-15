#include "e2e5_gai_bounds.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

int main(int argc, char** argv) {
  std::array<std::uint8_t, 100> data{};
  auto write = [&](std::size_t offset, std::int32_t value) {
    std::memcpy(data.data() + offset, &value, sizeof(value));
  };
  std::memcpy(data.data(), "gai\0", 4);
  write(4, 1);
  write(16, 1);  // bounds right
  write(20, 1);  // bounds bottom
  write(24, 1);  // one frame
  write(48, 56);
  write(52, 4);
  assert(srhd_awa::platform::e2e5_gai_bounds::Validate(data.data(), data.size()) == nullptr);
  write(52, 200);
  assert(std::strcmp(srhd_awa::platform::e2e5_gai_bounds::Validate(data.data(), data.size()),
                     "frame bounds") == 0);
  write(52, 4);
  write(32, 60);
  write(36, 16);
  write(60, 1);  // sequence count
  write(68, 16); // block extends past sequence table
  assert(std::strcmp(srhd_awa::platform::e2e5_gai_bounds::Validate(data.data(), data.size()),
                     "sequence block") == 0);
  write(36, 40);
  write(68, 16); // block starts at byte 76
  write(76, 1);  // one sequence frame
  write(80, 1);  // one-past source index for the sole image frame
  assert(std::strcmp(srhd_awa::platform::e2e5_gai_bounds::Validate(data.data(), data.size()),
                     "sequence frame index") == 0);
  assert(srhd_awa::platform::e2e5_gai_bounds::Validate(data.data(), data.size(), true) == nullptr);
  write(80, 2);
  assert(std::strcmp(srhd_awa::platform::e2e5_gai_bounds::Validate(data.data(), data.size(), true),
                     "sequence frame index") == 0);
  write(80, 0);
  assert(std::strcmp(srhd_awa::platform::e2e5_gai_bounds::Validate(data.data(), 5),
                     "buffer size") == 0);
  if (argc > 1) {
    std::ifstream stream(argv[1], std::ios::binary);
    assert(stream.good());
    const std::vector<std::uint8_t> original{
        std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    assert(std::strcmp(srhd_awa::platform::e2e5_gai_bounds::Validate(
                           original.data(), original.size()),
                       "sequence frame index") == 0);
    assert(srhd_awa::platform::e2e5_gai_bounds::Validate(
               original.data(), original.size(), true) == nullptr);
  }
}
