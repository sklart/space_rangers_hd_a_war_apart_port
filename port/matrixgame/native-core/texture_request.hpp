// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "TextureFlags.hpp"
#include "texture_asset.hpp"
#include <string>
namespace Base {class ParamParser;}
namespace srhd_awa::matrixgame {
std::wstring ReplaceTextureFileExt(std::wstring_view,std::wstring_view);
std::uint32_t ParseOriginalTextureFlags(const Base::ParamParser&);
struct TextureRequest {
  std::wstring name,path;
  std::uint32_t flags{};
  static TextureRequest Resolve(std::wstring_view);
  // Honors original Compressed semantics: STRG requires TF_COMPRESSED and vice versa.
  TextureAsset Load() const;
};
}
