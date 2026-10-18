// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
namespace srhd_awa::matrixgame {
// DDS BC blocks remain compressed, including every supplied mip level.
enum class TextureFormat : std::uint32_t { RGBA8, BC1, BC2, BC3 };
struct TextureLevel {
  std::uint32_t width{},height{},pitch{};
  std::vector<std::byte> bytes;
};
struct TextureAsset {
  TextureFormat format{TextureFormat::RGBA8};
  std::vector<TextureLevel> levels;
  static TextureAsset Decode(std::span<const std::byte>);
  // Exact CFile/PKG path, without material parameters or extension fallback.
  static TextureAsset Load(std::wstring_view);
};
}
