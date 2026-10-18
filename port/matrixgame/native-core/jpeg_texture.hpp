// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "texture_asset.hpp"
namespace srhd_awa::matrixgame {
// Budget is cumulative requested codec data, separate from owned RGBA bytes/RSS.
TextureAsset DecodeJpegTexture(std::span<const std::byte>,std::size_t codec_budget=64*1024*1024);
}
