// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "debug_font_backend.hpp"
#include <functional>
#include <span>
#include <vector>
#include <memory>
namespace srhd_awa::matrixgame::debug_font::native {
struct Glyph {std::uint32_t codepoint,advance,x,y,width,height;bool operator==(const Glyph&)const=default;};
struct Data {
    std::uint32_t width{},height{},line_height{},ascent{},descent{},average{},maximum{},charset{};
    std::vector<Glyph> glyphs;
    std::vector<std::uint16_t> map;
    std::vector<std::byte> rgba;
    static std::shared_ptr<const Data> Load(std::span<const std::byte>);
};
struct Quad {float left,top,right,bottom,u0,v0,u1,v1;std::uint32_t color;};
struct Layout {std::vector<Quad> quads;TextRect bounds;int result{};};
Layout Place(const Data&,const wchar_t*,int,TextRect,std::uint32_t,std::uint32_t);
struct Atlas {virtual ~Atlas()=default;};
struct Font;
struct Session {
    std::shared_ptr<const Data> data;
    std::function<std::shared_ptr<Atlas>(const Data&)> upload;
    std::function<void(const std::shared_ptr<Atlas>&,std::span<const Quad>)> draw;
    std::vector<std::weak_ptr<Font>> fonts;
    void Close()noexcept;
};
struct Font:std::enable_shared_from_this<Font> {
    std::shared_ptr<Session> session;
    std::shared_ptr<Atlas> atlas;
    bool lost{};
    static std::shared_ptr<Font> Create(std::shared_ptr<Session>);
    void Lost();void Reset();
    void Rebind(std::shared_ptr<Session>);
    int Draw(const wchar_t*,int,TextRect&,std::uint32_t,std::uint32_t);
};
class Provider {
public:virtual ~Provider()=default;virtual std::shared_ptr<Session> DebugFontSession()=0;
};
platform3g::DeviceResult CreateFor(platform3g::Device&,ID3DXFont**);
void Release(ID3DXFont*)noexcept;
Font& Find(ID3DXFont*);
void ResetFor(platform3g::Device&,ID3DXFont*);
std::span<const std::byte> OriginalAsset();
}
