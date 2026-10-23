// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
namespace srhd_awa::matrixgame::vertex_layout {
// Serialized original vertex layout bits, not device capability declarations.
inline constexpr std::uint32_t Position=0x002,TransformedPosition=0x004,PositionBlend1=0x006;
inline constexpr std::uint32_t Normal=0x010,Diffuse=0x040,Specular=0x080;
inline constexpr std::uint32_t Texture1=0x100,Texture2=0x200,Texture3=0x300;
constexpr std::uint32_t TexcoordSize3(unsigned index){return 1u<<(index*2+16);}
}
