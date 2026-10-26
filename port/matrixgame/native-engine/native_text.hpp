// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "text_backend.hpp"
#include <span>
#include <map>
#include <memory>
#include <vector>
namespace srhd_awa::matrixgame::text_backend::native {
struct Glyph {unsigned cp{},advance{},x{},y{},width{},height{};std::vector<std::byte> coverage;};
struct Data {
 unsigned id{},size{},height{},ascent{},descent{};std::vector<std::uint16_t> map;std::vector<Glyph> glyphs;std::map<std::uint32_t,int> kerning;
 static std::shared_ptr<const Data> Load(std::span<const std::byte>);
 const Glyph& Lookup(wchar_t)const;
};
// Output is D3DFMT_A8R8G8B8 bytes, including the original SRCALPHA alpha channel.
int Draw(const Data&,const wchar_t*,int,Rect&,unsigned,unsigned,std::span<std::byte>,unsigned,unsigned,unsigned);
class Provider {public:virtual ~Provider()=default;virtual std::span<const std::byte> TextFontAsset(unsigned)=0;};
std::span<const std::byte> OriginalAsset(unsigned);
}
