#pragma once

#include <cstdint>
#include <vector>

namespace m23_test {
inline void Put32(std::vector<std::uint8_t>* bytes, std::size_t at, std::uint32_t value) {
  for (unsigned i = 0; i != 4; ++i) (*bytes)[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
inline std::vector<std::uint8_t> Rle(int width, int height, bool alpha) {
  std::vector<std::uint8_t> result(16);
  for (int row = 0; row < height; ++row) {
    result.push_back(static_cast<std::uint8_t>(0x80 | width));
    if (alpha) for (int x = 0; x < width; ++x) result.push_back(static_cast<std::uint8_t>(row ? 192 : 64 + x * 80));
    result.push_back(0);
  }
  Put32(&result, 0, static_cast<std::uint32_t>(result.size() - 16));
  Put32(&result, 4, static_cast<std::uint32_t>(width));
  Put32(&result, 8, static_cast<std::uint32_t>(height));
  return result;
}
inline std::vector<std::uint8_t> AftFixture() {
  constexpr char32_t codes[] = {U' ', U'A', U'B', U'C', U'Ё', U'я'};
  std::vector<std::uint8_t> bytes(32 + 6 * 64);
  bytes[0] = 'a'; bytes[1] = 'f'; bytes[2] = 't';
  Put32(&bytes, 4, 1); Put32(&bytes, 8, 6); Put32(&bytes, 12, 3); Put32(&bytes, 20, 5);
  for (int i = 0; i < 6; ++i) {
    const auto at = 32 + i * 64;
    Put32(&bytes, at, static_cast<std::uint32_t>(codes[i]));
    Put32(&bytes, at + 4, i == 5 ? static_cast<std::uint32_t>(-1) : 1);
    Put32(&bytes, at + 8, i == 0 ? 2 : 3);
    Put32(&bytes, at + 12, i == 4 ? 2 : 1);
    if (i == 0) continue;
    for (int pass = 0; pass < 2; ++pass) {
      const bool alpha = pass == 1;
      if ((i == 1 && alpha) || (i == 2 && !alpha)) continue;
      const auto plane = at + 16 + pass * 24;
      Put32(&bytes, plane, i == 5 ? static_cast<std::uint32_t>(-1) : 0);
      Put32(&bytes, plane + 4, i == 4 ? static_cast<std::uint32_t>(-2) : 0);
      Put32(&bytes, plane + 8, 2); Put32(&bytes, plane + 12, 2);
      Put32(&bytes, plane + 16, static_cast<std::uint32_t>(bytes.size()));
      const auto encoded = Rle(2, 2, alpha);
      Put32(&bytes, plane + 20, static_cast<std::uint32_t>(encoded.size()));
      bytes.insert(bytes.end(), encoded.begin(), encoded.end());
    }
  }
  return bytes;
}
}  // namespace m23_test
