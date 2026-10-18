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
    std::array<std::byte,20> translucent_white{};
    translucent_white.fill(std::byte{0xee});
    for(int row=0;row<2;++row) for(int x=0;x<2;++x) {
      const auto offset=row*12+x*4;
      for(int c=0;c<3;++c) translucent_white[offset+c]=std::byte{255};
      translucent_white[offset+3]=std::byte{128};
    }
    const std::array<std::byte,4> opaque_white{std::byte{255},std::byte{255},std::byte{255},std::byte{255}};
    renderer.SetTextureRGBA8(1,2,2,12,translucent_white);
    renderer.SetTextureRGBA8(2,1,1,4,opaque_white);
    std::array<MeshSubmission,2> submissions{};
    submissions[0].range={0,1,0,3,0,3};
    submissions[1].range={1,4,3,3,0,3};
    for(auto& submission:submissions)
      for(int i=0;i<4;++i) submission.draw.clip_from_model[i*5]=submission.draw.normal_from_model[i*5]=1;
    submissions[0].texture_slot=1; submissions[1].texture_slot=2;
    submissions[1].sampling=TextureSampling::LinearClamp;
    submissions[0].draw.diffuse={1,0,0,1};
    submissions[1].draw.diffuse={0,0,1,1};
    std::vector<std::byte> rgba(64*64*4);
    renderer.DrawFrame(submissions,rgba);
    std::size_t red=0,blue=0;
    for(std::size_t i=0;i<rgba.size();i+=4) {
      if(rgba[i]!=std::byte{0} && rgba[i+2]==std::byte{0}) {
        if(rgba[i+3]!=std::byte{128}) return 3;
        ++red;
      }
      if(rgba[i]==std::byte{0} && rgba[i+2]!=std::byte{0}) {
        if(rgba[i+3]!=std::byte{255}) return 4;
        ++blue;
      }
    }
    if(!red || !blue || red+blue>=64*64) {
      std::fprintf(stderr,"GPU batch readback FAIL red=%zu blue=%zu\n",red,blue); return 2;
    }
    std::printf("GPU batch readback red=%zu blue=%zu; geometry fixture only; battle_executed=false\n",red,blue);
    return 0;
  } catch(const std::exception& error) { std::fprintf(stderr,"deko probe: %s\n",error.what()); return 1; }
}
