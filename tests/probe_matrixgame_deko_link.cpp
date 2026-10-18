// SPDX-License-Identifier: GPL-2.0-or-later
// Linkable future physical probe. Building it is NOT evidence of a GPU frame.
#include "mesh_renderer.hpp"
#include "mesh_shaders.hpp"
#include <array>
#include <cstdio>
#include <vector>
using namespace srhd_awa::matrixgame;
int main() {
  try {
    DekoMeshRenderer renderer(64,64,std::as_bytes(std::span(vertex_dksh)),std::as_bytes(std::span(fragment_dksh)));
    const std::array mesh{
      MeshVertex{{-.7f,-.7f,.5f},{0,0,1},{0,0}},
      MeshVertex{{ .7f,-.7f,.5f},{0,0,1},{1,0}},
      MeshVertex{{  0.f,.7f,.5f},{0,0,1},{.5f,1}}};
    const std::array<std::uint16_t,3> indices{0,1,2};
    renderer.SetMesh(mesh,indices);
    MeshDraw draw;
    for(int i=0;i<4;++i) draw.clip_from_model[i*5]=draw.normal_from_model[i*5]=1;
    std::vector<std::byte> rgba(64*64*4);
    renderer.Draw(draw,rgba);
    std::size_t lit=0;
    for(std::size_t i=0;i<rgba.size();i+=4) if(rgba[i]!=std::byte{0}) ++lit;
    if(!lit || lit==64*64) { std::fprintf(stderr,"GPU readback coverage FAIL %zu\n",lit); return 2; }
    std::printf("GPU readback nonblack_pixels=%zu; geometry fixture only; battle_executed=false\n",lit);
    return 0;
  } catch(const std::exception& error) { std::fprintf(stderr,"deko probe: %s\n",error.what()); return 1; }
}
