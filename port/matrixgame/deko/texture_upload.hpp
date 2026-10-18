// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
namespace srhd_awa::matrixgame {
struct Rgba8Upload {
  std::uint32_t width{},height{},source_pitch{},staging_pitch{},staging_bytes{};
  std::uint64_t source_required{};
};
inline Rgba8Upload ValidateRgba8Upload(std::uint32_t width,std::uint32_t height,
    std::uint32_t source_pitch,std::size_t source_bytes) {
  if(!width || !height || width>2048 || height>2048 || source_pitch<std::uint64_t(width)*4)
    throw std::invalid_argument("RGBA8 upload dimensions/pitch");
  const auto required=std::uint64_t(height-1)*source_pitch+std::uint64_t(width)*4;
  if(required>source_bytes) throw std::out_of_range("short RGBA8 source rows");
  const auto staging_pitch=(width*4+31u)&~31u;
  return {width,height,source_pitch,staging_pitch,staging_pitch*height,required};
}
inline void PackRgba8Rows(const Rgba8Upload& layout,std::span<const std::byte> source,
    std::span<std::byte> staging) {
  // Revalidate a caller-provided plan; do not trust arbitrary plan fields.
  const auto checked=ValidateRgba8Upload(layout.width,layout.height,layout.source_pitch,source.size());
  if(checked.staging_pitch!=layout.staging_pitch || checked.staging_bytes!=layout.staging_bytes ||
      checked.source_required!=layout.source_required || staging.size()<checked.staging_bytes)
    throw std::invalid_argument("RGBA8 staging plan/extent");
  const auto src=reinterpret_cast<std::uintptr_t>(source.data());
  const auto dst=reinterpret_cast<std::uintptr_t>(staging.data());
  if(src<=dst ? dst-src<checked.source_required : src-dst<checked.staging_bytes)
    throw std::invalid_argument("overlapping RGBA8 source/staging");
  std::fill_n(staging.data(),checked.staging_bytes,std::byte{0});
  for(std::uint32_t y=0;y<checked.height;++y)
    std::memcpy(staging.data()+std::size_t(y)*checked.staging_pitch,
        source.data()+std::size_t(y)*checked.source_pitch,checked.width*4);
}
} // namespace srhd_awa::matrixgame
