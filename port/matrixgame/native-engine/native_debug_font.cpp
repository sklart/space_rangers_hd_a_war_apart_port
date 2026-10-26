// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_debug_font.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
namespace srhd_awa::matrixgame::debug_font::native {
namespace {
std::map<ID3DXFont*,std::shared_ptr<Font>> handles;
int Coordinate(std::int64_t value){if(value<std::numeric_limits<int>::min() || value>std::numeric_limits<int>::max())throw std::length_error("debug text coordinate overflow");return int(value);}
}
std::shared_ptr<const Data> Data::Load(std::span<const std::byte> bytes){
    if(bytes.size()<48 || std::memcmp(bytes.data(),"MGDF25A1",8))throw std::invalid_argument("original debug font asset signature");
    std::size_t offset=8;auto word=[&](){if(offset+4>bytes.size())throw std::invalid_argument("truncated debug font asset");
        std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(bytes[offset++])<<(8*i);return value;};
    auto data=std::make_shared<Data>();data->width=word();data->height=word();data->line_height=word();data->ascent=word();data->descent=word();data->average=word();data->maximum=word();data->charset=word();
    const auto count=word();if(!count || count>65535 || !data->width || !data->height || data->width>4096 || data->height>4096 || data->line_height!=25 || data->ascent+data->descent!=25)
        throw std::invalid_argument("original debug font metrics/extent");
    data->glyphs.reserve(count);
    for(unsigned i=0;i<count;++i){Glyph glyph{word(),word(),word(),word(),word(),word()};
        if(glyph.codepoint>65535 || glyph.advance>128 || !glyph.width || !glyph.height || glyph.x>data->width || glyph.y>data->height || glyph.width>data->width-glyph.x || glyph.height>data->height-glyph.y)
            throw std::invalid_argument("original debug font glyph rectangle");data->glyphs.push_back(glyph);}
    data->map.reserve(65536);for(unsigned cp=0;cp<65536;++cp){if(offset+2>bytes.size())throw std::invalid_argument("debug font character map truncated");
        const auto index=unsigned(bytes[offset])|(unsigned(bytes[offset+1])<<8);offset+=2;if(index!=0xffff && index>=count)throw std::invalid_argument("debug font character map index");data->map.push_back(std::uint16_t(index));}
    const std::size_t size=std::size_t(data->width)*data->height*4;if(bytes.size()-offset!=size)throw std::invalid_argument("debug font atlas payload size");
    data->rgba.assign(bytes.begin()+offset,bytes.end());bool ink=false;
    for(std::size_t i=0;i<size;i+=4){if(data->rgba[i]!=std::byte{255} || data->rgba[i+1]!=std::byte{255} || data->rgba[i+2]!=std::byte{255})throw std::invalid_argument("debug font coverage RGB");ink|=data->rgba[i+3]!=std::byte{};}
    if(!ink)throw std::invalid_argument("empty original debug font atlas");return data;
}
Layout Place(const Data& data,const wchar_t* text,int count,TextRect bounds,std::uint32_t flags,std::uint32_t color){
    if(!text || count < -1 || (flags!=0x124 && flags!=0x420 && flags!=0x120))throw platform3g::Unsupported("native debug font text/count/format");
    if(count==-1){count=0;while(text[count]){if(count>=65536)throw std::length_error("debug text length");++count;}}
    if(count>65536)throw std::length_error("debug text length");
    Layout result;result.bounds=bounds;if(!count || !text[0]){
        if(flags==0x124)result.result=int((std::int64_t(bounds.bottom)-bounds.top)/2);
        if(flags==0x420){result.bounds.right=bounds.left;result.bounds.bottom=bounds.top;}return result;
    }
    const int vertical=flags==0x124?int((std::int64_t(bounds.bottom)-bounds.top-int(data.line_height))/2):0;
    std::int64_t pen=bounds.left;int width=0;
    for(int n=0;n<count && text[n];++n){auto cp=std::uint32_t(text[n]);
        if(cp>65535 || (cp>=0xd800 && cp<=0xdfff))throw platform3g::Unsupported("native debug font requires BMP scalar text");
        if(cp=='\r' || cp=='\n' || cp=='\t')continue;
        const auto index=data.map.at(cp);if(index==0xffff)throw platform3g::Unsupported("glyph outside original MS Sans Serif coverage; font linking required");
        const auto& glyph=data.glyphs.at(index);
        if(flags!=0x420){const auto x=float(pen-2),y=float(std::int64_t(bounds.top)+vertical-2);
            result.quads.push_back({x,y,x+glyph.width,y+glyph.height,float(glyph.x)/data.width,float(glyph.y)/data.height,
                float(glyph.x+glyph.width)/data.width,float(glyph.y+glyph.height)/data.height,color});}
        pen+=glyph.advance;if(pen>std::numeric_limits<int>::max() || pen<std::numeric_limits<int>::min())throw std::length_error("debug text coordinate overflow");
        if(cp!=' ')width=Coordinate(pen-bounds.left);
    }
    if(flags==0x420){result.bounds.right=Coordinate(std::int64_t(bounds.left)+width);result.bounds.bottom=Coordinate(std::int64_t(bounds.top)+data.line_height);}
    result.result=Coordinate(std::int64_t(data.line_height)+vertical);return result;
}
std::shared_ptr<Font> Font::Create(std::shared_ptr<Session> session){
    if(!session || !session->data || !session->upload || !session->draw)throw platform3g::Unsupported("native debug font device session unavailable");
    auto font=std::make_shared<Font>();font->session=std::move(session);font->atlas=font->session->upload(*font->session->data);
    if(!font->atlas)throw std::runtime_error("native debug font upload returned no atlas");font->session->fonts.push_back(font);return font;
}
void Font::Lost(){atlas.reset();lost=true;}
void Font::Reset(){
    if(!session->upload)throw platform3g::Unsupported("native debug font device expired");
    auto replacement=session->upload(*session->data);if(!replacement)throw std::runtime_error("native debug font reset returned no atlas");atlas=std::move(replacement);lost=false;
}
void Font::Rebind(std::shared_ptr<Session> next){
    if(!next || !next->upload || !next->draw || !next->data)throw platform3g::Unsupported("replacement debug font session unavailable");
    const auto& old=*session->data;const auto& replacement=*next->data;
    if(old.width!=replacement.width || old.height!=replacement.height || old.line_height!=replacement.line_height || old.ascent!=replacement.ascent || old.descent!=replacement.descent || old.average!=replacement.average || old.maximum!=replacement.maximum || old.charset!=replacement.charset || old.glyphs!=replacement.glyphs || old.map!=replacement.map || old.rgba!=replacement.rgba)
        throw platform3g::Unsupported("replacement device changed original debug font");
    auto image=next->upload(replacement);if(!image)throw std::runtime_error("replacement debug font upload returned no atlas");
    next->fonts.push_back(shared_from_this());session=std::move(next);atlas=std::move(image);lost=false;
}
int Font::Draw(const wchar_t* text,int count,TextRect& rect,std::uint32_t flags,std::uint32_t color){
    if(!session->upload || !session->draw)throw platform3g::Unsupported("native debug font device expired");
    auto layout=Place(*session->data,text,count,rect,flags,color);
    if(flags!=0x420 && !layout.quads.empty()){if(lost || !atlas)throw platform3g::Unsupported("native debug font resources lost; reset required");session->draw(atlas,layout.quads);}
    rect=layout.bounds;return layout.result;
}
void Session::Close()noexcept{upload={};draw={};for(auto& value:fonts)if(auto font=value.lock();font && font->session.get()==this){font->atlas.reset();font->lost=true;}fonts.clear();}
platform3g::DeviceResult CreateFor(platform3g::Device& device,ID3DXFont** out){
    if(!out || *out)throw std::invalid_argument("native font output must be empty");
    auto* provider=dynamic_cast<Provider*>(&device);if(!provider)throw platform3g::Unsupported("native debug font provider unavailable");
    auto font=Font::Create(provider->DebugFontSession());auto* token=reinterpret_cast<ID3DXFont*>(font.get());handles.emplace(token,std::move(font));*out=token;return 0;
}
Font& Find(ID3DXFont* token){auto it=handles.find(token);if(it==handles.end())throw std::invalid_argument("unregistered native debug font token");return *it->second;}
void ResetFor(platform3g::Device& device,ID3DXFont* token){
    auto* provider=dynamic_cast<Provider*>(&device);if(!provider)throw platform3g::Unsupported("replacement native debug font provider unavailable");
    auto& font=Find(token);auto session=provider->DebugFontSession();if(session==font.session)font.Reset();else font.Rebind(std::move(session));
}
void Release(ID3DXFont* token)noexcept{if(token && !handles.erase(token))std::terminate();}
}
