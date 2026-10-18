// SPDX-License-Identifier: GPL-2.0-or-later
// Byte-preserving upload layout only. No GPU sampling or native asset decode.
#include "../port/matrixgame/deko/texture_upload.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <limits>
#include <vector>
using namespace srhd_awa::matrixgame;
template<class E,class F> void Reject(F f) {bool rejected=false;try{f();}catch(const E&){rejected=true;}assert(rejected);}
int main() {
  unsigned cases=0;
  for(unsigned w:{1u,2u,7u,8u,9u,15u,16u,17u,31u,32u,33u,255u,2048u})
    for(unsigned h:{1u,2u,3u}) for(unsigned padding:{0u,1u,13u,32u}) {
      const unsigned pitch=w*4+padding;
      std::vector<std::byte> source((h-1)*pitch+w*4);
      for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w*4;++x)
        source[y*pitch+x]=std::byte((y*53+x*17+31)&255);
      const auto saved=source;
      auto plan=ValidateRgba8Upload(w,h,pitch,source.size());
      assert(plan.staging_pitch>=w*4 && plan.staging_pitch%32==0);
      assert(plan.source_required==source.size());
      std::vector<std::byte> staging(plan.staging_bytes+16,std::byte{0xa5});
      PackRgba8Rows(plan,source,std::span(staging).first(plan.staging_bytes));
      for(unsigned y=0;y<h;++y) {
        for(unsigned x=0;x<w*4;++x) assert(staging[y*plan.staging_pitch+x]==saved[y*pitch+x]);
        for(unsigned x=w*4;x<plan.staging_pitch;++x) assert(staging[y*plan.staging_pitch+x]==std::byte{0});
      }
      for(std::size_t i=plan.staging_bytes;i<staging.size();++i) assert(staging[i]==std::byte{0xa5});
      assert(source==saved); ++cases;
    }
  const auto maximum=ValidateRgba8Upload(2048,2048,8192,16777216);
  assert(maximum.staging_bytes==16777216 && maximum.source_required==16777216);
  Reject<std::invalid_argument>([]{ValidateRgba8Upload(0,1,4,4);});
  Reject<std::invalid_argument>([]{ValidateRgba8Upload(1,0,4,4);});
  Reject<std::invalid_argument>([]{ValidateRgba8Upload(2049,1,8196,8196);});
  Reject<std::invalid_argument>([]{ValidateRgba8Upload(1,2049,4,8196);});
  Reject<std::invalid_argument>([]{ValidateRgba8Upload(2,1,7,8);});
  Reject<std::out_of_range>([]{ValidateRgba8Upload(2,2,12,19);});
  Reject<std::out_of_range>([]{ValidateRgba8Upload(1,2048,UINT32_MAX,16384);});
  std::array<std::byte,20> source{};std::array<std::byte,64> staging{};
  const auto plan=ValidateRgba8Upload(2,2,12,source.size());
  auto corrupt=plan;corrupt.staging_bytes=1;
  staging.fill(std::byte{0x5a});const auto before=staging;
  Reject<std::invalid_argument>([&]{PackRgba8Rows(corrupt,source,staging);});
  Reject<std::invalid_argument>([&]{PackRgba8Rows(plan,source,std::span(staging).first(63));});
  assert(staging==before);
  std::array<std::byte,128> overlap{};overlap.fill(std::byte{0x7b});const auto old=overlap;
  Reject<std::invalid_argument>([&]{PackRgba8Rows(plan,std::span(overlap).first(20),std::span(overlap).first(64));});
  Reject<std::invalid_argument>([&]{PackRgba8Rows(plan,std::span(overlap).subspan(32,20),std::span(overlap).first(64));});
  assert(overlap==old);
  std::printf("RGBA8 row upload PASS layouts=%u exact_bytes_alpha=preserved padding=zero overflow=guarded GPU_executed=false\n",cases);
}
