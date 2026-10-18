// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../native-core/texture_asset.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
namespace srhd_awa::matrixgame {
struct TextureMipUpload {
  std::uint32_t width,height,row_bytes,rows,offset,pitch;
};
struct TextureAssetUpload {
  std::vector<TextureMipUpload> levels;
  std::uint32_t bytes{};
};
inline TextureAssetUpload PlanTextureAssetUpload(const TextureAsset& asset) {
  if(asset.levels.empty()||asset.levels.size()>12)throw std::invalid_argument("texture level count");
  const auto type=static_cast<std::uint32_t>(asset.format);
  if(type>3)throw std::invalid_argument("texture format");
  const unsigned block=type?(type==1?8:16):4,extent=type?4:1;
  auto w=asset.levels[0].width,h=asset.levels[0].height;
  if(!w||!h||w>2048||h>2048||asset.levels.size()>std::bit_width(std::max(w,h)))
    throw std::invalid_argument("GPU texture dimensions/mips");
  TextureAssetUpload result;
  for(const auto& level:asset.levels) {
    const auto row=((w+extent-1)/extent)*block,rows=(h+extent-1)/extent;
    if(level.width!=w||level.height!=h||level.pitch<row||
        std::uint64_t(rows-1)*level.pitch+row>level.bytes.size())throw std::invalid_argument("texture mip rows");
    const auto offset=(result.bytes+255u)&~255u,pitch=(row+31u)&~31u;
    if(std::uint64_t(offset)+std::uint64_t(pitch)*rows>64*1024*1024)throw std::length_error("texture staging budget");
    result.levels.push_back({w,h,row,rows,offset,pitch});result.bytes=offset+pitch*rows;
    w=std::max(1u,w/2);h=std::max(1u,h/2);
  }
  return result;
}
inline void PackTextureAsset(const TextureAsset& asset,std::span<std::byte> output) {
  const auto plan=PlanTextureAssetUpload(asset);
  if(output.size()<plan.bytes)throw std::length_error("short texture staging");
  const auto address=reinterpret_cast<std::uintptr_t>(output.data());
  for(const auto& level:asset.levels) {
    const auto source=reinterpret_cast<std::uintptr_t>(level.bytes.data());
    if(source<=address?address-source<level.bytes.size():source-address<plan.bytes)
      throw std::invalid_argument("texture staging alias");
  }
  std::fill_n(output.data(),plan.bytes,std::byte{});
  for(std::size_t i=0;i<plan.levels.size();++i) {
    const auto& p=plan.levels[i];const auto& source=asset.levels[i];
    for(std::uint32_t y=0;y<p.rows;++y)
      std::memcpy(output.data()+p.offset+std::size_t(y)*p.pitch,source.bytes.data()+std::size_t(y)*source.pitch,p.row_bytes);
  }
}
}
