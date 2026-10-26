// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_debug_font.hpp"
// Exact original Windows D3DX bake, linked as immutable binary data by objcopy.
extern "C" {extern const unsigned char _binary_original_font_bin_start[],_binary_original_font_bin_end[];}
namespace srhd_awa::matrixgame::debug_font::native {
std::span<const std::byte> OriginalAsset(){
    return {reinterpret_cast<const std::byte*>(_binary_original_font_bin_start),
        std::size_t(reinterpret_cast<std::uintptr_t>(_binary_original_font_bin_end)-reinterpret_cast<std::uintptr_t>(_binary_original_font_bin_start))};
}
}
