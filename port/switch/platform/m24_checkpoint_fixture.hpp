#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace srhd_awa::platform::m24_checkpoint_fixture {

inline void Put32(std::vector<std::uint8_t>* bytes, std::size_t at, std::uint32_t value) {
  for (unsigned i{}; i < 4; ++i) (*bytes)[at + i] = value >> (i * 8);
}
inline std::vector<std::uint8_t> Bmp(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  std::vector<std::uint8_t> bytes(58);
  bytes[0] = 'B'; bytes[1] = 'M'; Put32(&bytes, 2, bytes.size());
  Put32(&bytes, 10, 54); Put32(&bytes, 14, 40); Put32(&bytes, 18, 1); Put32(&bytes, 22, 1);
  bytes[26] = 1; bytes[28] = 24; Put32(&bytes, 34, 4);
  bytes[54] = blue; bytes[55] = green; bytes[56] = red;
  return bytes;
}
inline std::vector<std::uint8_t> Gi(std::uint16_t color) {
  std::vector<std::uint8_t> bytes(104); std::memcpy(bytes.data(), "gi\0", 3);
  Put32(&bytes, 4, 1); Put32(&bytes, 16, 2); Put32(&bytes, 20, 2);
  Put32(&bytes, 24, 0xf800); Put32(&bytes, 28, 0x07e0); Put32(&bytes, 32, 0x001f);
  Put32(&bytes, 44, 1); Put32(&bytes, 64, 96); Put32(&bytes, 68, 8);
  Put32(&bytes, 80, 2); Put32(&bytes, 84, 2);
  for (std::size_t at = 96; at < 104; at += 2) {
    bytes[at] = color; bytes[at + 1] = color >> 8;
  }
  return bytes;
}
inline std::vector<std::uint8_t> Gai() {
  const auto first = Gi(0xf800), second = Gi(0x07e0);
  constexpr std::size_t directory = 64;
  const auto second_at = directory + first.size(), table = second_at + second.size();
  std::vector<std::uint8_t> bytes(table + 36); std::memcpy(bytes.data(), "gai\0", 4);
  Put32(&bytes, 4, 1); Put32(&bytes, 16, 2); Put32(&bytes, 20, 2);
  Put32(&bytes, 24, 2); Put32(&bytes, 32, table); Put32(&bytes, 36, 36);
  Put32(&bytes, 48, directory); Put32(&bytes, 52, first.size());
  Put32(&bytes, 56, second_at); Put32(&bytes, 60, second.size());
  std::memcpy(bytes.data() + directory, first.data(), first.size());
  std::memcpy(bytes.data() + second_at, second.data(), second.size());
  Put32(&bytes, table, 1); Put32(&bytes, table + 8, 16); Put32(&bytes, table + 16, 2);
  Put32(&bytes, table + 20, 0); Put32(&bytes, table + 24, 10);
  Put32(&bytes, table + 28, 1); Put32(&bytes, table + 32, 10);
  return bytes;
}
inline std::vector<std::uint8_t> Rle(bool alpha) {
  std::vector<std::uint8_t> bytes(16);
  for (int row{}; row < 2; ++row) {
    bytes.push_back(0x82);
    if (alpha) for (int x{}; x < 2; ++x) bytes.push_back(row ? 192 : 64 + x * 80);
    bytes.push_back(0);
  }
  Put32(&bytes, 0, bytes.size() - 16);
  Put32(&bytes, 4, 2); Put32(&bytes, 8, 2);
  return bytes;
}
inline std::vector<std::uint8_t> Aft() {
  constexpr char32_t codes[] = {U' ', U'A', U'B', U'C', U'Ё', U'я'};
  std::vector<std::uint8_t> bytes(32 + 6 * 64);
  bytes[0] = 'a'; bytes[1] = 'f'; bytes[2] = 't';
  Put32(&bytes, 4, 1); Put32(&bytes, 8, 6); Put32(&bytes, 12, 3); Put32(&bytes, 20, 5);
  for (int i{}; i < 6; ++i) {
    const auto at = 32 + i * 64;
    Put32(&bytes, at, static_cast<std::uint32_t>(codes[i]));
    Put32(&bytes, at + 4, i == 5 ? static_cast<std::uint32_t>(-1) : 1);
    Put32(&bytes, at + 8, i == 0 ? 2 : 3);
    Put32(&bytes, at + 12, i == 4 ? 2 : 1);
    if (i == 0) continue;
    for (int pass{}; pass < 2; ++pass) {
      const bool alpha = pass == 1;
      if ((i == 1 && alpha) || (i == 2 && !alpha)) continue;
      const auto plane = at + 16 + pass * 24;
      Put32(&bytes, plane, i == 5 ? static_cast<std::uint32_t>(-1) : 0);
      Put32(&bytes, plane + 4, i == 4 ? static_cast<std::uint32_t>(-2) : 0);
      Put32(&bytes, plane + 8, 2); Put32(&bytes, plane + 12, 2);
      Put32(&bytes, plane + 16, bytes.size());
      const auto encoded = Rle(alpha);
      Put32(&bytes, plane + 20, encoded.size());
      bytes.insert(bytes.end(), encoded.begin(), encoded.end());
    }
  }
  return bytes;
}

}  // namespace srhd_awa::platform::m24_checkpoint_fixture
