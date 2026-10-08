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
std::vector<std::uint8_t> Bmp() { std::vector<std::uint8_t>b(58); b[0]='B';b[1]='M';W(&b,2,58);W(&b,10,54);W(&b,14,40);W(&b,18,1);W(&b,22,1);b[26]=1;b[28]=24;W(&b,34,4);b[54]=0;b[55]=0;b[56]=255;return b; }
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
  AlphaBitmap bitmap; std::string error; const auto bmp=Bmp();
  if (!Expect(bitmap.Load(bmp.data(),static_cast<std::int32_t>(bmp.size()),&error),"load") || !Expect(bitmap.width()==1&&bitmap.height()==1&&bitmap.resident_bytes()>0,"metadata")) return false;
  std::uint16_t pixel=0x001f; if (!Expect(bitmap.Draw(&pixel,1,1,1,0,0,{0,0,1,1},&error),"draw")) return false;
  const std::uint8_t bad[]={'B','M'};
  if (!Expect(!bitmap.Load(bad,sizeof bad,&error)&&!bitmap.loaded()&&bitmap.resident_bytes()==0,"failure clears")) return false;
  return Expect(bitmap.Load(bmp.data(),static_cast<std::int32_t>(bmp.size()),&error),"reload");
}
}
int main(){if(!TestRleOrderAndValidation()||!TestLifecycle())return 1;std::puts("ALPHA BITMAP CPU PASS");return 0;}
