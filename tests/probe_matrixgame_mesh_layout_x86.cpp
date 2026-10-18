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
 std::printf("native mesh boundary layout PASS vertex=%zu normal=%zu uv=%zu triangle16=%zu; GPU_executed=false\n",
 sizeof(SVOVertex),offsetof(SVOVertex,n),offsetof(SVOVertex,tu),sizeof(SVOIndices16));
}
