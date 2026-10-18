// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
namespace srhd_awa::matrixgame {
// DDS BC blocks remain compressed, including every supplied mip level.
enum class TextureFormat : std::uint32_t { RGBA8, BC1, BC2, BC3 };
enum class TextureMipPolicy { PreserveSupplied,OneLevel,CompleteBox };
struct TextureLevel {
  std::uint32_t width{},height{},pitch{};
  std::vector<std::byte> bytes;
};
struct TextureAsset {
  TextureFormat format{TextureFormat::RGBA8};
  std::array<std::uint8_t,4> clear_pixel{};
  // PNG gAMA numerator, pending original texture-loader color correction.
  // Raw bitmap decoding retains the source bytes until PrepareMips succeeds.
  std::uint32_t png_gamma{};
  std::vector<TextureLevel> levels;
  static TextureAsset Decode(std::span<const std::byte>);
  // Exact CFile/PKG path, without material parameters or extension fallback.
  static TextureAsset Load(std::wstring_view);
  // Complete original-style POT/BOX layout, preserving supplied POT payloads.
  // Budget counts owned input + new output + temporary mip work until commit.
  void PrepareMips(TextureMipPolicy,std::size_t budget=64*1024*1024);
};
}
