// SPDX-License-Identifier: GPL-2.0-or-later
#include "texture_asset.hpp"
#include "bitmap_limits.hpp"
#include "BC.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
namespace srhd_awa::matrixgame {
namespace {
using namespace DirectX;
unsigned BlockBytes(TextureFormat format) {
  switch(format){case TextureFormat::RGBA8:return 4;case TextureFormat::BC1:return 8;
    case TextureFormat::BC2:case TextureFormat::BC3:return 16;default:throw std::invalid_argument("mip texture format");}
}
std::size_t RowBytes(TextureFormat format,unsigned w){return std::size_t(format==TextureFormat::RGBA8?w:(w+3)/4)*BlockBytes(format);}
unsigned Rows(TextureFormat format,unsigned h){return format==TextureFormat::RGBA8?h:(h+3)/4;}
void DecodeBlock(TextureFormat format,XMVECTOR* out,const std::byte* data) {
  const auto* bytes=reinterpret_cast<const std::uint8_t*>(data);
  if(format==TextureFormat::BC1)D3DXDecodeBC1(out,bytes);
  else if(format==TextureFormat::BC2)D3DXDecodeBC2(out,bytes);
  else D3DXDecodeBC3(out,bytes);
  // The original D3DX CPU filter uses BC1's three-color RGB interpretation
  // for BC2/3 endpoints too. Keep this compatibility local to mip filtering;
  // retained blocks and the GPU's native BC2/3 sampler remain unchanged.
  if(format!=TextureFormat::BC1) {
    alignas(16) XMVECTOR rgb[16];D3DXDecodeBC1(rgb,bytes+8);
    for(unsigned i=0;i<16;++i)out[i]=XMVectorSetW(rgb[i],XMVectorGetW(out[i]));
  }
}
void EncodeBlock(TextureFormat format,std::byte* data,const XMVECTOR* pixels) {
  auto* out=reinterpret_cast<std::uint8_t*>(data);
  if(format==TextureFormat::BC1)D3DXEncodeBC1(out,pixels,.5f,BC_FLAGS_NONE);
  else if(format==TextureFormat::BC2)D3DXEncodeBC2(out,pixels,BC_FLAGS_NONE);
  else D3DXEncodeBC3(out,pixels,BC_FLAGS_NONE);
}
void FillBC(TextureFormat format,const TextureLevel& source,TextureLevel& target,
    std::array<std::uint8_t,4> clear,bool box) {
  const auto block_bytes=BlockBytes(format);
  const auto transparent=XMVectorSet(clear[0]/255.f,clear[1]/255.f,clear[2]/255.f,clear[3]/255.f);
  for(unsigned by=0;by<(target.height+3)/4;++by)for(unsigned bx=0;bx<(target.width+3)/4;++bx) {
    alignas(16) XMVECTOR colors[16],cache[4][16];unsigned count=0;
    std::array<std::array<unsigned,2>,4> keys{};
    const auto pixel=[&](unsigned x,unsigned y) {
      if(x>=source.width||y>=source.height)return transparent;
      const std::array key{x/4,y/4};unsigned i=0;
      for(;i<count;++i)if(keys[i]==key)break;
      if(i==count){if(count==4)throw std::logic_error("BC mip source footprint");keys[count++]=key;
        DecodeBlock(format,cache[i],source.bytes.data()+std::size_t(key[1])*source.pitch+key[0]*block_bytes);}
      return cache[i][(y%4)*4+x%4];
    };
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x) {
      const auto dx=(bx*4+x)%target.width,dy=(by*4+y)%target.height;
      if(!box)colors[y*4+x]=pixel(dx,dy);
      else {
        const auto x0=std::min(dx*2,source.width-1),x1=std::min(x0+1,source.width-1);
        const auto y0=std::min(dy*2,source.height-1),y1=std::min(y0+1,source.height-1);
        colors[y*4+x]=XMVectorScale(XMVectorAdd(XMVectorAdd(XMVectorAdd(
          pixel(x0,y0),pixel(x1,y0)),pixel(x0,y1)),pixel(x1,y1)),.25f);
      }
    }
    EncodeBlock(format,target.bytes.data()+std::size_t(by)*target.pitch+bx*block_bytes,colors);
  }
}
void FillRGBA(const TextureLevel& source,TextureLevel& target,std::array<std::uint8_t,4> clear,bool box) {
  for(unsigned y=0;y<target.height;++y)for(unsigned x=0;x<target.width;++x)for(unsigned c=0;c<4;++c) {
    unsigned value=clear[c];
    if(!box) {if(x<source.width&&y<source.height)value=std::to_integer<unsigned>(source.bytes[std::size_t(y)*source.pitch+x*4+c]);}
    else {
      const auto x0=std::min(x*2,source.width-1),x1=std::min(x0+1,source.width-1),y0=std::min(y*2,source.height-1),y1=std::min(y0+1,source.height-1);
      const auto p=[&](unsigned px,unsigned py){return std::to_integer<unsigned>(source.bytes[std::size_t(py)*source.pitch+px*4+c]);};
      value=(p(x0,y0)+p(x1,y0)+p(x0,y1)+p(x1,y1)+2)/4;
      if(source.width==1||source.height==1) {
        // D3DX's generic BOX path rounds each sequential float addition,
        // including duplicated samples on the one-pixel axis. The 2-D
        // RGBA fast path above instead averages integer channel values.
        constexpr float scale=1.f/255.f;
        float sum=float(p(x0,y0))*scale;
        sum+=float(p(x1,y0))*scale;
        sum+=float(p(x0,y1))*scale;
        sum+=float(p(x1,y1))*scale;
        const float normalized=sum*.25f;
        value=unsigned(double(normalized)*255.0+.5);
      }
    }
    target.bytes[std::size_t(y)*target.pitch+x*4+c]=std::byte(value);
  }
}
}
void TextureAsset::CompressBC1(std::size_t budget) {
  if(format!=TextureFormat::RGBA8 || png_gamma)throw std::invalid_argument("BC1 input must be prepared RGBA");
  PrepareMips(TextureMipPolicy::PreserveSupplied,budget);
  std::size_t used=0;for(const auto& source:levels)used+=source.bytes.size();
  std::vector<TextureLevel> result;result.reserve(levels.size());
  for(const auto& source:levels) {
    const auto pitch=((source.width+3)/4)*8,rows=(source.height+3)/4;
    const auto size=std::size_t(pitch)*rows;
    if(size>budget-used)throw std::length_error("BC1 conversion peak budget");used+=size;
    TextureLevel target{source.width,source.height,pitch,std::vector<std::byte>(size)};
    for(unsigned by=0;by<rows;++by)for(unsigned bx=0;bx<(source.width+3)/4;++bx) {
      alignas(16) DirectX::XMVECTOR colors[16];
      for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x) {
        const auto sx=std::min(bx*4+x,source.width-1),sy=std::min(by*4+y,source.height-1);
        const auto* p=source.bytes.data()+std::size_t(sy)*source.pitch+sx*4;
        colors[y*4+x]=DirectX::XMVectorSet(unsigned(p[0])/255.f,unsigned(p[1])/255.f,unsigned(p[2])/255.f,unsigned(p[3])/255.f);
      }
      EncodeBlock(TextureFormat::BC1,target.bytes.data()+std::size_t(by)*pitch+bx*8,colors);
    }
    result.push_back(std::move(target));
  }
  levels.swap(result);format=TextureFormat::BC1;
}
void TextureAsset::PrepareMips(TextureMipPolicy policy,std::size_t budget){PrepareMips(policy,budget,0);}
void TextureAsset::PrepareMips(TextureMipPolicy policy,std::size_t budget,std::uint32_t level_count) {
  if(levels.empty())throw std::invalid_argument("empty mip image");
  if(budget>::matrixgame::native::BitmapLimit)throw std::length_error("mip image budget");
  auto w=levels[0].width,h=levels[0].height;
  if(!w||!h||w>8192||h>8192||levels.size()>std::bit_width(std::max(w,h)))throw std::invalid_argument("mip dimensions/count");
  std::size_t used=0;
  for(const auto& level:levels) {
    const auto row=RowBytes(format,w);const auto rows=Rows(format,h);
    if(level.width!=w||level.height!=h||level.pitch<row||std::uint64_t(rows-1)*level.pitch+row>level.bytes.size())throw std::invalid_argument("mip input rows");
    if(level.bytes.size()>budget-used)throw std::length_error("mip input ownership budget");used+=level.bytes.size();
    w=std::max(1u,w/2);h=std::max(1u,h/2);
  }
  if(policy==TextureMipPolicy::PreserveSupplied){if(level_count)throw std::invalid_argument("explicit count with preserved mips");return;}
  if(policy!=TextureMipPolicy::OneLevel&&policy!=TextureMipPolicy::CompleteBox)throw std::invalid_argument("mip policy");
  std::array<std::uint8_t,256> gamma_lut{};
  if(png_gamma) {
    if(format!=TextureFormat::RGBA8)throw std::invalid_argument("PNG gamma texture format");
    const double exponent=100000.0/(2.2*png_gamma);
    for(unsigned c=0;c<256;++c)gamma_lut[c]=std::uint8_t(std::clamp(std::pow(c/255.0,exponent)*255.0+.5,0.0,255.0));
  }
  const unsigned minimum=format==TextureFormat::RGBA8?1u:4u;
  w=std::max(minimum,std::bit_ceil(levels[0].width));h=std::max(minimum,std::bit_ceil(levels[0].height));
  const auto full=std::bit_width(std::max(w,h));
  if(level_count>full || (policy==TextureMipPolicy::OneLevel && level_count>1))throw std::invalid_argument("explicit mip count");
  const auto count=policy==TextureMipPolicy::OneLevel?1u:level_count?level_count:full;
  std::vector<TextureLevel> result(count);
  const auto previous=[&](std::size_t i)->const TextureLevel& {return result[i].bytes.empty()?levels[i]:result[i];};
  for(unsigned i=0;i<count;++i) {
    if(png_gamma||i>=levels.size()||levels[i].width!=w||levels[i].height!=h||
        (format!=TextureFormat::RGBA8&&(w<4||h<4))) {
      const auto row=RowBytes(format,w),bytes=row*Rows(format,h);
      if(bytes>budget-used)throw std::length_error("mip replacement peak budget");used+=bytes;
      result[i]={w,h,std::uint32_t(row),std::vector<std::byte>(bytes)};
      const bool box=i>=levels.size();const auto& source=box?previous(i-1):levels[i];
      if(format==TextureFormat::RGBA8)FillRGBA(source,result[i],clear_pixel,box);
      else FillBC(format,source,result[i],clear_pixel,box);
      // Supplied levels start in the PNG file's color space. Generated levels
      // already use the corrected previous level, so never apply gamma twice.
      if(png_gamma&&!box)for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(unsigned c=0;c<3;++c) {
        auto& value=result[i].bytes[std::size_t(y)*result[i].pitch+x*4+c];
        value=std::byte(gamma_lut[std::to_integer<unsigned>(value)]);
      }
    }
    w=std::max(1u,w/2);h=std::max(1u,h/2);
  }
  // Move unchanged supplied buffers only after every allocation/filter succeeded.
  for(unsigned i=0;i<count;++i)if(result[i].bytes.empty())result[i]=std::move(levels[i]);
  levels.swap(result);
  png_gamma=0;
}
}
