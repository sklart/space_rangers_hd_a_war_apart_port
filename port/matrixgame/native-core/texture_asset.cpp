// SPDX-License-Identifier: GPL-2.0-or-later
// Native texture input boundary for the real MatrixGame formats. No D3D shim.
#include "texture_asset.hpp"
#include "CBitmap.hpp"
#include "CFile.hpp"
#include "CStorage.hpp"
#include "bitmap_limits.hpp"
#include "jpeg_texture.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>
namespace srhd_awa::matrixgame {
namespace {
using ::matrixgame::native::BitmapLimit;
using ::matrixgame::native::PngInputLimit;
std::uint32_t Word(std::span<const std::byte> b,std::size_t at) {
  if(at>b.size()||b.size()-at<4)throw std::invalid_argument("short texture word");
  return ::matrixgame::native::BitmapRead<std::uint32_t>(b.data()+at);
}
bool Magic(std::span<const std::byte> b,const char* value,std::size_t n=4) {
  return b.size()>=n&&!std::memcmp(b.data(),value,n);
}
unsigned Channel(std::uint32_t pixel,std::uint32_t mask,unsigned absent=0) {
  if(!mask)return absent;
  const auto shift=std::countr_zero(mask);const auto range=mask>>shift;
  return unsigned((std::uint64_t((pixel&mask)>>shift)*255+range/2)/range);
}
TextureAsset Dds(std::span<const std::byte> b) {
  if(b.size()<128||Word(b,4)!=124||Word(b,76)!=32)throw std::invalid_argument("DDS header size");
  const auto flags=Word(b,8),h0=Word(b,12),w0=Word(b,16),count=std::max(1u,Word(b,28));
  if(!w0||!h0||w0>8192||h0>8192||Word(b,24)>1||Word(b,112)||
      (flags&0x800000)||!(Word(b,108)&0x1000))throw std::invalid_argument("unsupported DDS dimensions/type");
  if(count>std::bit_width(std::max(w0,h0)))throw std::invalid_argument("DDS mip count");
  const auto pf=Word(b,80),fourcc=Word(b,84),bits=Word(b,88);
  TextureAsset result;unsigned block=0;
  std::array<std::uint32_t,4> masks{Word(b,92),Word(b,96),Word(b,100),Word(b,104)};
  if(pf&4) {
    if(fourcc==0x31545844){result.format=TextureFormat::BC1;block=8;}
    else if(fourcc==0x33545844){result.format=TextureFormat::BC2;block=16;}
    else if(fourcc==0x35545844){result.format=TextureFormat::BC3;block=16;}
    else throw std::invalid_argument("unsupported DDS FourCC");
  }else {
    if((bits!=8&&bits!=16&&bits!=24&&bits!=32)||!(pf&(0x40|0x20000|2)))
      throw std::invalid_argument("unsupported DDS pixel format");
    if((pf&0x40)&&(pf&0x20000))throw std::invalid_argument("ambiguous DDS color format");
    if((pf&2)&&(pf&(0x40|0x20000)))throw std::invalid_argument("ambiguous DDS alpha format");
    std::uint32_t used=0;
    for(auto mask:masks) {
      const auto shifted=mask?mask>>std::countr_zero(mask):0;
      if((mask&used)||(bits<32&&(mask>>bits))||(shifted&(shifted+1)))throw std::invalid_argument("DDS channel masks");
      used|=mask;
    }
    if(((pf&1)&&!masks[3])||((pf&0x40)&&(!masks[0]||!masks[1]||!masks[2]))||
        ((pf&0x20000)&&(!masks[0]||masks[1]||masks[2]))||((pf&2)&&(!masks[3]||masks[0]||masks[1]||masks[2])))
      throw std::invalid_argument("missing DDS channel mask");
    if(!(pf&(1|2)))masks[3]=0;
  }
  std::size_t at=128,owned=0;auto w=w0,h=h0;
  for(std::uint32_t level=0;level<count;++level) {
    const std::size_t rows=block?(h+3)/4:h;
    const std::size_t packed=block?std::size_t((w+3)/4)*block:std::size_t(w)*(bits/8);
    const std::size_t source_pitch=!block&&!level&&(flags&8)?Word(b,20):packed;
    if(source_pitch<packed||source_pitch>PngInputLimit/rows||source_pitch*rows>b.size()-at)
      throw std::invalid_argument("short DDS mip rows");
    const auto target_pitch=block?packed:std::size_t(w)*4,bytes=target_pitch*rows;
    if(bytes>BitmapLimit-owned)throw std::length_error("texture retained byte budget");
    TextureLevel mip{w,h,std::uint32_t(target_pitch),std::vector<std::byte>(bytes)};
    for(std::size_t y=0;y<rows;++y) {
      const auto* source=b.data()+at+y*source_pitch;auto* target=mip.bytes.data()+y*target_pitch;
      if(block)std::memcpy(target,source,packed);
      else for(std::uint32_t x=0;x<w;++x) {
        std::uint32_t pixel=0;std::memcpy(&pixel,source+x*(bits/8),bits/8);
        const auto r=Channel(pixel,masks[0]);
        target[x*4]=std::byte(r);
        target[x*4+1]=std::byte((pf&0x20000)?r:Channel(pixel,masks[1]));
        target[x*4+2]=std::byte((pf&0x20000)?r:Channel(pixel,masks[2]));
        target[x*4+3]=std::byte(Channel(pixel,masks[3],255));
      }
    }
    result.levels.push_back(std::move(mip));owned+=bytes;at+=source_pitch*rows;
    w=std::max(1u,w/2);h=std::max(1u,h/2);
  }
  if(at!=b.size())throw std::invalid_argument("trailing DDS payload");
  return result;
}
TextureAsset Image(std::span<const std::byte> b) {
  if(b.size()>=2&&b[0]==std::byte{255}&&b[1]==std::byte{216})return DecodeJpegTexture(b);
  if(Magic(b,"DDS "))return Dds(b);
  if(Magic(b,"\x89PNG\r\n\x1a\n",8)) {
    CBitmap bitmap;
    if(!bitmap.LoadRGBAFromPNG(const_cast<std::byte*>(b.data()),int(b.size())))throw std::invalid_argument("texture PNG decode");
    TextureAsset result;TextureLevel level{std::uint32_t(bitmap.SizeX()),std::uint32_t(bitmap.SizeY()),std::uint32_t(bitmap.Pitch()),{}};
    const auto* pixels=reinterpret_cast<const std::byte*>(bitmap.Data());
    level.bytes.assign(pixels,pixels+std::size_t(level.pitch)*level.height);result.levels.push_back(std::move(level));return result;
  }
  throw std::invalid_argument("unsupported texture payload");
}
}
TextureAsset TextureAsset::Decode(std::span<const std::byte> b) {
  if(b.empty()||b.size()>PngInputLimit)throw std::length_error("texture input budget");
  if(!Magic(b,"STRG"))return Image(b);
  Base::CBuf input;input.Add(b.data(),b.size());Base::CStorage storage;storage.Load(input);
  auto* bytes=storage.GetBuf(L"0",L"0",Base::ST_BYTE);
  if(!bytes||!bytes->GetArraysCount())throw std::invalid_argument("missing STRG texture 0/0 bytes");
  const auto count=bytes->GetArrayLength(0);
  if(!count||count>PngInputLimit)throw std::length_error("STRG texture input budget");
  return Image({reinterpret_cast<const std::byte*>(bytes->GetFirst<std::uint8_t>(0)),count});
}
TextureAsset TextureAsset::Load(std::wstring_view path) {
  Base::CFile file{std::wstring(path)};file.OpenRead();const auto size=file.Size();
  if(size<=0||std::uint64_t(size)>PngInputLimit)throw std::length_error("texture file input budget");
  std::vector<std::byte> bytes(std::size_t(size),std::byte{});file.Read(bytes.data(),bytes.size());return Decode(bytes);
}
}
