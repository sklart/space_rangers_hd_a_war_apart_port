// SPDX-License-Identifier: GPL-2.0-or-later
#include "png_texture.hpp"
#include "CBitmap.hpp"
namespace srhd_awa::matrixgame {
PngTextureSize UploadPngTexture(DekoMeshRenderer& renderer,std::uint32_t slot,std::wstring_view path) {
  CBitmap bitmap;
  if(!bitmap.LoadRGBAFromPNG(path))throw std::runtime_error("MatrixGame PNG decode failed");
  const auto width=std::uint32_t(bitmap.SizeX()),height=std::uint32_t(bitmap.SizeY()),pitch=std::uint32_t(bitmap.Pitch());
  renderer.SetTextureRGBA8(slot,width,height,pitch,std::as_bytes(std::span(bitmap.Data(),std::size_t(pitch)*height)));
  return {width,height};
}
}
