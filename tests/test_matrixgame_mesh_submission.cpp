// SPDX-License-Identifier: GPL-2.0-or-later
// Native draw-field translation and bounds only. No GPU or battle is simulated.
#include "../port/matrixgame/deko/mesh_submission.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <limits>
#include <vector>
using namespace srhd_awa::matrixgame;
struct Group {
  std::int32_t m_Surface{},m_Base{};
  union {std::int32_t m_VerMinIndex{};std::int32_t m_IBase;};
  std::int32_t m_VerCnt{},m_TriCnt{},m_TriStart{};
};
template<class E,class F> void Reject(F f) {bool rejected=false;try{f();}catch(const E&){rejected=true;}assert(rejected);}
int main() {
  Group g; g.m_Surface=5;g.m_Base=-2;g.m_VerMinIndex=4;g.m_VerCnt=3;g.m_TriCnt=2;g.m_TriStart=1;
  auto range=VectorObjectRange(g,5,2);
  assert(range.surface==5 && range.base_vertex==3 && range.first_index==5 && range.index_count==6 && range.minimum_vertex==4 && range.vertex_count==3);
  std::array<std::uint16_t,12> indices{0,0,0,0,0,4,5,6,6,5,4,0};
  ValidateMeshRange(range,indices,10);
  g.m_Surface=6;g.m_Base=5;g.m_IBase=-4;g.m_TriCnt=1;g.m_TriStart=std::numeric_limits<std::int32_t>::min();
  range=VectorObjectRange(g,2,1);
  assert(range.surface==6 && range.base_vertex==7 && range.first_index==5 && range.index_count==3 && range.minimum_vertex==0 && range.vertex_count==3);
  indices[5]=2;indices[6]=0;indices[7]=1;
  ValidateMeshRange(range,indices,10);
  const std::array<std::uint16_t,3> negative_base{7,8,9};
  ValidateMeshRange({0,-7,0,3,7,3},negative_base,3);
  // Never alter indices to absorb the base vertex.
  assert(negative_base[0]==7 && negative_base[1]==8 && negative_base[2]==9);
  Reject<std::out_of_range>([&]{ValidateMeshRange({0,-8,0,3,7,3},negative_base,3);});
  Reject<std::out_of_range>([&]{ValidateMeshRange({0,-6,0,3,7,3},negative_base,3);});
  Reject<std::out_of_range>([&]{ValidateMeshRange({0,-7,0,3,8,2},negative_base,3);});
  Reject<std::out_of_range>([&]{ValidateMeshRange({0,-7,1,3,7,3},negative_base,3);});
  Reject<std::out_of_range>([&]{ValidateMeshRange({0,-7,0,2,7,3},negative_base,3);});
  const std::array<std::uint16_t,5> padded_buffer{65535,65535,0,1,2};
  ValidateMeshRange({0,0,2,3,0,3},padded_buffer,3);
  ValidateMeshRange({},std::span<const std::uint16_t>{},0);
  g.m_Base=std::numeric_limits<std::int32_t>::max();
  Reject<std::overflow_error>([&]{VectorObjectRange(g,1,0);});
  g.m_Base=0;g.m_IBase=std::numeric_limits<std::int32_t>::min();
  range=VectorObjectRange(g,0,0);assert(range.first_index==2147483648u);
  Reject<std::out_of_range>([&]{ValidateMeshRange(range,indices,10);});
  g.m_TriCnt=std::numeric_limits<std::int32_t>::max();
  Reject<std::overflow_error>([&]{VectorObjectRange(g,0,0);});
  g.m_TriCnt=-1;Reject<std::invalid_argument>([&]{VectorObjectRange(g,0,0);});
  g.m_TriCnt=1;g.m_VerCnt=-1;Reject<std::invalid_argument>([&]{VectorObjectRange(g,0,0);});
  g.m_VerCnt=1;Reject<std::invalid_argument>([&]{VectorObjectRange(g,-1,0);});
  Reject<std::invalid_argument>([&]{VectorObjectRange(g,0,-1);});
  // A direct CPU vertex-selection oracle for 2048 groups across both native
  // branches; checks the addressed vertex sequence without calling the helper.
  std::uint32_t random=12345;auto next=[&]{random=random*1664525u+1013904223u;return random;};
  std::vector<std::uint16_t> buffer(1024);
  for(int iteration=0;iteration<2048;++iteration) {
    const int vb=next()%64,ib=next()%32;
    g.m_Surface=next()%10;g.m_Base=int(next()%128)-32;
    g.m_VerCnt=1+next()%16;g.m_TriCnt=1+next()%5;g.m_TriStart=next()%16;
    const bool optimized=(next()&1)!=0;
    const int minimum=optimized ? 0 : int(next()%16);
    const int first=optimized ? 1+int(next()%32) : g.m_TriStart*3;
    g.m_VerMinIndex=optimized ? -first : minimum;
    // Keep the effective base nonnegative; negative base is covered above.
    if(g.m_Base+vb<0) g.m_Base=0;
    for(int i=0;i<g.m_TriCnt*3;++i) buffer[first+ib+i]=minimum+next()%g.m_VerCnt;
    auto translated=VectorObjectRange(g,vb,ib);
    ValidateMeshRange(translated,buffer,256);
    assert(translated.surface==g.m_Surface && translated.index_count==std::uint32_t(g.m_TriCnt*3));
    for(int i=0;i<g.m_TriCnt*3;++i) {
      const auto native_vertex=g.m_Base+vb+buffer[first+ib+i];
      const auto gpu_vertex=translated.base_vertex+buffer[translated.first_index+i];
      assert(native_vertex==gpu_vertex);
    }
  }
  std::puts("VectorObject draw translation PASS branches=2 randomized_groups=2048; GPU_executed=false");
}
