// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_text.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
namespace srhd_awa::matrixgame::text_backend::native {
std::shared_ptr<const Data> Data::Load(std::span<const std::byte> bytes){
 if(bytes.size()<32 || std::memcmp(bytes.data(),"MGTEXT01",8))throw std::invalid_argument("original UI font signature");
 std::size_t at=8;auto word=[&](){if(bytes.size()-at<4)throw std::invalid_argument("truncated UI font");unsigned value=0;for(unsigned k=0;k<4;++k)value|=unsigned(bytes[at++])<<(8*k);return value;};
 auto d=std::make_shared<Data>();d->id=word();d->size=word();d->height=word();d->ascent=word();d->descent=word();auto count=word();
 if(d->id>3 || d->size!=(d->id?11+d->id:10) || d->height!=d->size || d->ascent>d->height || d->descent>d->height || d->ascent+d->descent!=d->height || !count || count>65534)throw std::invalid_argument("original UI font metrics");
 d->map.reserve(65536);for(unsigned cp=0;cp<65536;++cp){if(bytes.size()-at<2)throw std::invalid_argument("truncated UI map");unsigned i=unsigned(bytes[at])|(unsigned(bytes[at+1])<<8);at+=2;if(i!=65535 && i>=count)throw std::invalid_argument("UI glyph map");d->map.push_back(i);}
 auto kernCount=word();if(kernCount>65536)throw std::invalid_argument("UI kerning count");for(unsigned i=0;i<kernCount;++i){auto a=word(),b=word();auto amount=std::int32_t(word());if(a>65535 || b>65535 || amount < -64 || amount>64)throw std::invalid_argument("UI kerning pair");d->kerning.emplace((a<<16)|b,amount);}
 bool ink=false;for(unsigned n=0;n<count;++n){Glyph g;g.cp=word();g.advance=word();g.x=word();g.y=word();g.width=word();g.height=word();
 if(g.cp>65535 || g.advance>60 || g.x>64 || g.y>64 || g.width>64-g.x || g.height>64-g.y || bytes.size()-at<std::size_t(g.width)*g.height)throw std::invalid_argument("UI glyph bounds");
 auto end=at+std::size_t(g.width)*g.height;g.coverage.assign(bytes.begin()+at,bytes.begin()+end);at=end;for(auto b:g.coverage)ink|=b!=std::byte{};d->glyphs.push_back(std::move(g));}
 if(at!=bytes.size() || !ink)throw std::invalid_argument("UI font payload/empty coverage");return d;
}
const Glyph& Data::Lookup(wchar_t cp)const{auto code=std::uint32_t(cp);if(code>65535 || map.at(code)==65535)throw platform3g::Unsupported("UI glyph outside original font; font linking required");return glyphs.at(map[code]);}
int Draw(const Data& d,const wchar_t* text,int count,Rect& rect,unsigned flags,unsigned color,std::span<std::byte> output,unsigned w,unsigned h,unsigned pitch){
 if((flags&0x400) && flags!=0x520)throw platform3g::Unsupported("UI CALCRECT format outside original font measurement");
 if(!(flags&0x100))throw platform3g::Unsupported("UI clipped layout outside original text requests");
 if(!text || count < -1 || (flags&~(0x100u|0x400u|0x20u|0x10u|3u|4u|8u)) || (flags&3)==3 || (flags&12)==12)throw platform3g::Unsupported("native UI text format");
 if(count==-1){count=0;while(text[count]){if(count==65536)throw std::length_error("UI text length");++count;}}if(count>65536)throw std::length_error("UI text length");
 if(count>1)for(int i=0;i<count && text[i];++i){auto cp=std::uint32_t(text[i]);if((cp>=0x300 && cp<=0x36f) || (cp>=0x590 && cp<=0x8ff) || cp==0x200c || cp==0x200d)throw platform3g::Unsupported("UI complex script shaping required");}
 if(!count || !text[0]){if(flags&0x400){rect.right=rect.left;rect.bottom=rect.top;return 0;}return flags&4?int((std::int64_t(rect.bottom)-rect.top)/2):flags&8?int(std::int64_t(rect.bottom)-rect.top):0;}
 struct Line{std::vector<const Glyph*> glyphs;int width{};};std::vector<Line> lines;
 const auto bound=std::int64_t(rect.right)-rect.left;
 auto paragraph=[&](std::vector<const Glyph*> glyphs){
  if(glyphs.empty()){lines.push_back({});return;}
  std::vector<int> prefix(glyphs.size()+1);for(std::size_t i=0;i<glyphs.size();++i){prefix[i+1]=prefix[i]+int(glyphs[i]->advance);if(i){auto k=d.kerning.find((glyphs[i-1]->cp<<16)|glyphs[i]->cp);if(k!=d.kerning.end())prefix[i+1]+=k->second;}}
  auto measure=[&](const auto&,std::size_t first,std::size_t last){int width=prefix[last]-prefix[first];if(first && first<last){auto k=d.kerning.find((glyphs[first-1]->cp<<16)|glyphs[first]->cp);if(k!=d.kerning.end())width-=k->second;}return width;};
  for(std::size_t first=0;first<glyphs.size();){std::size_t end=glyphs.size(),next=end;
   if((flags&0x10) && !(flags&0x20) && measure(glyphs,first,end)>bound){
    std::size_t fit=first;int width=0;while(fit<glyphs.size()){int add=glyphs[fit]->advance;if(fit>first){auto k=d.kerning.find((glyphs[fit-1]->cp<<16)|glyphs[fit]->cp);if(k!=d.kerning.end())add+=k->second;}if(std::int64_t(width)+add>bound)break;width+=add;++fit;}
    if(fit==first)++fit;end=fit;next=fit;
    auto digit=[](unsigned cp){return cp>='0' && cp<='9';};
    for(auto split=fit;split>first;--split)if(split<glyphs.size() && split>first && digit(glyphs[split]->cp)!=digit(glyphs[split-1]->cp) && glyphs[split]->cp!=' ' && glyphs[split-1]->cp!=' '){end=next=split;break;}else if(glyphs[split-1]->cp==' ' && (split==glyphs.size() || glyphs[split]->cp!=' ')){end=split;next=split;break;}
    while(end>first && glyphs[end-1]->cp==' ')--end;
   }
   Line line;line.glyphs.assign(glyphs.begin()+first,glyphs.begin()+end);line.width=measure(glyphs,first,end);lines.push_back(std::move(line));first=next;
  }
 };
 std::vector<const Glyph*> current;
 for(int i=0;i<count && text[i];++i){auto cp=text[i];if(cp=='\r')continue;if(cp=='\n'){if(flags&0x20)continue;paragraph(std::move(current));current.clear();continue;}if(cp=='\t')continue;current.push_back(&d.Lookup(cp));}
 if(!current.empty() || lines.empty())paragraph(std::move(current));
 int maxWidth=0;for(auto& line:lines){int width=line.width;for(auto i=line.glyphs.rbegin();i!=line.glyphs.rend() && (*i)->cp==' ';++i)width-=(*i)->advance;maxWidth=std::max(maxWidth,width);}
 if(std::all_of(lines.begin(),lines.end(),[](const auto& line){return line.width==0;})){if(flags&0x400){rect.right=rect.left;rect.bottom=rect.top;return 0;}return flags&4?int((std::int64_t(rect.bottom)-rect.top)/2):flags&8?int(std::int64_t(rect.bottom)-rect.top):0;}
 const int height=int(lines.size()*d.height);if(flags&0x400){if(std::int64_t(rect.left)+maxWidth>INT32_MAX || std::int64_t(rect.top)+height>INT32_MAX)throw std::length_error("UI text coordinate");rect.right=rect.left+maxWidth;rect.bottom=rect.top+height;return height;}
 if(!w || !h || pitch<w*4 || std::uint64_t(pitch)*h>output.size())throw std::invalid_argument("UI text output extent");
 std::int64_t y=rect.top;if(flags&4)y+=(std::int64_t(rect.bottom)-rect.top-height)/2;if(flags&8)y=std::int64_t(rect.bottom)-height;
 for(const auto& line:lines){std::int64_t x=rect.left;int width=line.width;for(auto i=line.glyphs.rbegin();i!=line.glyphs.rend() && (*i)->cp==' ';++i)width-=(*i)->advance;
  if(flags&1)x+=(bound-maxWidth)/2+(maxWidth-width)/2;if(flags&2)x=std::int64_t(rect.right)-width;
  for(unsigned gi=0;gi<line.glyphs.size();++gi){auto* g=line.glyphs[gi];for(unsigned gy=0;gy<g->height;++gy)for(unsigned gx=0;gx<g->width;++gx){auto dx=x+g->x-2+gx,dy=y+g->y-2+gy;
   if(dx<0 || dy<0 || dx>=w || dy>=h || (!(flags&0x100) && (dx<rect.left || dx>=rect.right || dy<rect.top || dy>=rect.bottom)))continue;
   unsigned coverage=unsigned(g->coverage[gy*g->width+gx]);unsigned alpha=(coverage*(color>>24)+127)/255;auto* pixel=output.data()+std::size_t(dy)*pitch+std::size_t(dx)*4;
   for(unsigned c=0;c<4;++c){unsigned src=c==3?alpha:(color>>(c*8))&255;unsigned value=(src*alpha+unsigned(pixel[c])*(255-alpha)+127)/255;pixel[c]=std::byte(value);}}
   x+=g->advance;if(gi+1<line.glyphs.size()){auto k=d.kerning.find((g->cp<<16)|line.glyphs[gi+1]->cp);if(k!=d.kerning.end())x+=k->second;}}y+=d.height;
 }if(y-rect.top<INT32_MIN || y-rect.top>INT32_MAX)throw std::length_error("UI text result overflow");return int(y-rect.top);
}
}
