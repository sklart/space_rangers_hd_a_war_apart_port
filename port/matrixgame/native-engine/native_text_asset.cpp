// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_text.hpp"
extern "C" {
extern const unsigned char _binary_font0_bin_start[],_binary_font0_bin_end[];
extern const unsigned char _binary_font1_bin_start[],_binary_font1_bin_end[];
extern const unsigned char _binary_font2_bin_start[],_binary_font2_bin_end[];
extern const unsigned char _binary_font3_bin_start[],_binary_font3_bin_end[];
}
namespace srhd_awa::matrixgame::text_backend::native {
std::span<const std::byte> OriginalAsset(unsigned id){
 const unsigned char* starts[]={_binary_font0_bin_start,_binary_font1_bin_start,_binary_font2_bin_start,_binary_font3_bin_start};
 const unsigned char* ends[]={_binary_font0_bin_end,_binary_font1_bin_end,_binary_font2_bin_end,_binary_font3_bin_end};
 if(id>3)throw std::out_of_range("original UI font asset id");return {reinterpret_cast<const std::byte*>(starts[id]),std::size_t(reinterpret_cast<std::uintptr_t>(ends[id])-reinterpret_cast<std::uintptr_t>(starts[id]))};
}
}
