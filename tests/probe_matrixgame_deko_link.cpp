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
      MeshVertex{{0,0,0},{0,0,1},{0,0}}, // Unreferenced prefix; base vertex must be respected.
      MeshVertex{{-.9f,-.7f,.5f},{0,0,1},{0,0}},
      MeshVertex{{-.1f,-.7f,.5f},{0,0,1},{1,0}},
      MeshVertex{{-.5f, .7f,.5f},{0,0,1},{.5f,1}},
      MeshVertex{{ .1f,-.7f,.5f},{0,0,1},{0,0}},
      MeshVertex{{ .9f,-.7f,.5f},{0,0,1},{1,0}},
      MeshVertex{{ .5f, .7f,.5f},{0,0,1},{.5f,1}}};
    const std::array<std::uint16_t,6> indices{0,1,2,0,1,2};
    renderer.SetMesh(mesh,indices);
    std::array<MeshSubmission,2> submissions{};
    submissions[0].range={0,1,0,3,0,3};
    submissions[1].range={1,4,3,3,0,3};
    for(auto& submission:submissions)
      for(int i=0;i<4;++i) submission.draw.clip_from_model[i*5]=submission.draw.normal_from_model[i*5]=1;
    submissions[0].draw.diffuse={1,0,0,1};
    submissions[1].draw.diffuse={0,0,1,1};
    std::vector<std::byte> rgba(64*64*4);
    renderer.DrawFrame(submissions,rgba);
    std::size_t red=0,blue=0;
    for(std::size_t i=0;i<rgba.size();i+=4) {
      if(rgba[i]!=std::byte{0} && rgba[i+2]==std::byte{0}) ++red;
      if(rgba[i]==std::byte{0} && rgba[i+2]!=std::byte{0}) ++blue;
    }
    if(!red || !blue || red+blue>=64*64) {
      std::fprintf(stderr,"GPU batch readback FAIL red=%zu blue=%zu\n",red,blue); return 2;
    }
    std::printf("GPU batch readback red=%zu blue=%zu; geometry fixture only; battle_executed=false\n",red,blue);
    return 0;
  } catch(const std::exception& error) { std::fprintf(stderr,"deko probe: %s\n",error.what()); return 1; }
}
