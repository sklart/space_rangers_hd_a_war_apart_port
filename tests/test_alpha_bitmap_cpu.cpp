#include "alpha_bitmap_cpu.hpp"
#include "okgf_rle_bridge.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
using srhd_awa::platform::alpha_bitmap_cpu::AlphaBitmap;
using srhd_awa::platform::alpha_bitmap_cpu::ValidateRle;
using namespace srhd_awa::platform::okgf_rle_bridge;
bool Expect(bool value, const char* text) { if (!value) std::fprintf(stderr, "FAIL: %s\n", text); return value; }
void W(std::vector<std::uint8_t>* b, std::size_t at, std::uint32_t v) { for (int i=0;i<4;++i) (*b)[at+i]=static_cast<std::uint8_t>(v>>(8*i)); }
void BE16(std::vector<std::uint8_t>* b, std::size_t at, std::uint16_t v) { (*b)[at]=static_cast<std::uint8_t>(v>>8); (*b)[at+1]=static_cast<std::uint8_t>(v); }
void BE32(std::vector<std::uint8_t>* b, std::size_t at, std::uint32_t v) { for (int i=0;i<4;++i) (*b)[at+i]=static_cast<std::uint8_t>(v>>(24-8*i)); }
std::vector<std::uint8_t> Bmp() { std::vector<std::uint8_t>b(58); b[0]='B';b[1]='M';W(&b,2,58);W(&b,10,54);W(&b,14,40);W(&b,18,1);W(&b,22,1);b[26]=1;b[28]=24;W(&b,34,4);b[54]=0;b[55]=0;b[56]=255;return b; }
std::vector<std::uint8_t> PsdRgba() { std::vector<std::uint8_t>b(44); std::memcpy(b.data(),"8BPS",4); BE16(&b,4,1); BE16(&b,12,4); BE32(&b,14,1); BE32(&b,18,1); BE16(&b,22,8); BE16(&b,24,3); b[40]=255; b[41]=0; b[42]=0; b[43]=128; return b; }
std::vector<std::uint8_t> Rle(std::uint32_t width,std::uint32_t height,std::initializer_list<std::uint8_t> stream) { std::vector<std::uint8_t>b(16); W(&b,0,static_cast<std::uint32_t>(stream.size()));W(&b,4,width);W(&b,8,height);b.insert(b.end(),stream);return b; }
bool TestCorruptRle() {
  std::vector<std::uint8_t> short_header(15); if(!Expect(!ValidateRle(short_header.data(),short_header.size(),1,1,1),"truncated header"))return false;
  auto valid=Rle(1,1,{0x81,0xff,0}); if(!Expect(ValidateRle(valid.data(),valid.size(),1,1,1),"manual valid RLE"))return false;
  auto bad_dimensions=valid;W(&bad_dimensions,4,2);if(!Expect(!ValidateRle(bad_dimensions.data(),bad_dimensions.size(),1,1,1),"bad dimensions"))return false;
  auto bad_length=valid;bad_length[0]++;if(!Expect(!ValidateRle(bad_length.data(),bad_length.size(),1,1,1),"bad stream bytes"))return false;
  auto literal_overflow=Rle(1,1,{0x81,0});if(!Expect(!ValidateRle(literal_overflow.data(),literal_overflow.size(),1,1,1),"literal overflow"))return false;
  auto row_overflow=Rle(1,1,{0x02,0});if(!Expect(!ValidateRle(row_overflow.data(),row_overflow.size(),1,1,1),"row overflow"))return false;
  auto too_few_rows=Rle(1,2,{0x81,0xff,0});if(!Expect(!ValidateRle(too_few_rows.data(),too_few_rows.size(),1,2,1),"too few rows"))return false;
  auto trailing=Rle(1,1,{0x81,0xff,0,0});return Expect(!ValidateRle(trailing.data(),trailing.size(),1,1,1),"trailing bytes");
}
bool TestRleOrderAndValidation() {
  const std::uint8_t bgra[] = {0, 0, 255, 128};
  const auto build = [&bgra](auto fn, std::size_t literal) {
    const auto n=fn(bgra,4,1,1,nullptr); std::vector<std::uint8_t>b(static_cast<std::size_t>(n));
    return std::pair<std::vector<std::uint8_t>,std::size_t>{std::move(b),literal};
  };
  auto a=build(BuildAlphaBufFromBgra,1); auto ta=build(BuildTransAlphaBufFromBgra,2); auto t=build(BuildTransBufFromBgra,2);
  BuildAlphaBufFromBgra(bgra,4,1,1,a.first.data()); BuildTransAlphaBufFromBgra(bgra,4,1,1,ta.first.data()); BuildTransBufFromBgra(bgra,4,1,1,t.first.data());
  if (!Expect(ValidateRle(a.first.data(),a.first.size(),1,1,a.second),"alpha valid") || !Expect(ValidateRle(ta.first.data(),ta.first.size(),1,1,ta.second),"trans alpha valid") || !Expect(ValidateRle(t.first.data(),t.first.size(),1,1,t.second),"trans valid")) return false;
  auto corrupt=a.first; corrupt[0]++; if (!Expect(!ValidateRle(corrupt.data(),corrupt.size(),1,1,1),"RLE stream size corruption")) return false;
  std::uint16_t pixel=0x001f; const Rect clip{0,0,1,1};
  DrawAlphaBuf565Clip(&pixel,2,0,0,a.first.data(),clip); DrawTransAlphaBuf565Clip(&pixel,2,0,0,ta.first.data(),clip); DrawTransBuf565Clip(&pixel,2,0,0,t.first.data(),clip);
  return Expect(pixel == 0x780f,"alpha draw ordering");
}
bool TestLifecycle() {
  AlphaBitmap bitmap; std::string error; const auto bmp=Bmp(); const auto psd=PsdRgba();
  if (!Expect(bitmap.Load(psd.data(),static_cast<std::int32_t>(psd.size()),&error),"load") || !Expect(bitmap.width()==1&&bitmap.height()==1&&bitmap.resident_bytes()>0,"metadata")) return false;
  std::uint16_t pixel=0x001f; if (!Expect(bitmap.Draw(&pixel,1,1,1,0,0,{0,0,1,1},&error),"draw")) return false;
  std::vector<std::uint8_t> decoded; std::int32_t pitch{};
  // The RLE route is RGB565-backed, so the reconstructed red channel is the
  // upstream rounded value (239), not the original 8-bit source value (255).
  const bool decoded_ok=bitmap.DecodeToBGRA(&decoded,&pitch,&error) && pitch==4 && decoded.size()==4 && decoded[2]==239 && decoded[3]==128;
  if (!Expect(decoded_ok,"decode BGRA")) return false;
  const std::uint8_t bad[]={'B','M'};
  if (!Expect(!bitmap.Load(bad,sizeof bad,&error)&&!bitmap.loaded()&&bitmap.resident_bytes()==0,"failure clears")) return false;
  if (!Expect(!bitmap.DecodeToBGRA(&decoded,&pitch,&error)&&decoded.empty()&&pitch==0,"unloaded decode clears")) return false;
  return Expect(bitmap.Load(bmp.data(),static_cast<std::int32_t>(bmp.size()),&error),"reload");
}
}
int main(){if(!TestCorruptRle()||!TestRleOrderAndValidation()||!TestLifecycle())return 1;std::puts("ALPHA BITMAP CPU PASS");return 0;}
