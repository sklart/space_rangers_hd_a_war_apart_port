// SPDX-License-Identifier: GPL-2.0-or-later
// Actual pinned MatrixLib declarations; no resource loading or fake battle.
#include "MatrixGame.h"
#include "VectorObject.hpp"
#include "../port/matrixgame/deko/mesh_renderer.hpp"
#include <cstddef>
#include <cstdio>
using NativeMesh=srhd_awa::matrixgame::MeshVertex;
static_assert(sizeof(void*)==4,"Use the Windows x86 reference build");
static_assert(sizeof(SVOVertex)==sizeof(NativeMesh));
static_assert(offsetof(SVOVertex,v)==offsetof(NativeMesh,position));
static_assert(offsetof(SVOVertex,n)==offsetof(NativeMesh,normal));
static_assert(offsetof(SVOVertex,tu)==offsetof(NativeMesh,uv));
static_assert(offsetof(SVOVertex,tv)==offsetof(NativeMesh,uv)+sizeof(float));
static_assert(sizeof(SVOIndices16)==3*sizeof(std::uint16_t));
int main() {
 SVOUnion group{}; group.m_Surface=3; group.m_Base=5; group.m_VerMinIndex=2;
 group.m_VerCnt=4; group.m_TriCnt=3; group.m_TriStart=7;
 const auto normal=srhd_awa::matrixgame::VectorObjectRange(group,11,13);
 if(normal.surface!=3 || normal.base_vertex!=16 || normal.first_index!=34 || normal.index_count!=9 ||
    normal.minimum_vertex!=2 || normal.vertex_count!=4) return 2;
 group.m_IBase=-17;
 const auto optimized=srhd_awa::matrixgame::VectorObjectRange(group,11,13);
 if(optimized.base_vertex!=16 || optimized.first_index!=30 || optimized.index_count!=9 || optimized.minimum_vertex!=0) return 3;
 std::puts("native SVOUnion draw translation PASS branches=2; GPU_executed=false");
 std::printf("native mesh boundary layout PASS vertex=%zu normal=%zu uv=%zu triangle16=%zu; GPU_executed=false\n",
 sizeof(SVOVertex),offsetof(SVOVertex,n),offsetof(SVOVertex,tu),sizeof(SVOIndices16));
}
