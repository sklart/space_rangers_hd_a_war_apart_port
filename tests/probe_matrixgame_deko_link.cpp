// SPDX-License-Identifier: GPL-2.0-or-later
// Linkable future physical probe. Building it is NOT evidence of a GPU frame.
#include "mesh_renderer.hpp"
#include "vector_submission.hpp"
#include "mesh_shaders.hpp"
#include "config_unicode.hpp"
#include <array>
#include <cstdio>
#include <vector>
using namespace srhd_awa::matrixgame;
int main(int argc,char** argv) {
  try {
    DekoMeshRenderer renderer(64,64,std::as_bytes(std::span(vertex_dksh)),std::as_bytes(std::span(fragment_dksh)));
    if(argc==3 && std::string_view(argv[1])=="--geometry-only") {
      const auto path=::matrixgame::native::ConfigWide(argv[2]);
      const auto model=VectorGeometry::Load(path.c_str());
      UploadVectorGeometry(renderer,model);
      // Explicit geometry diagnostic, not a material/texture/battle render.
      // Use the model's actual first-frame bounds, preserving every vertex.
      const auto& frame=model.Frames()[0];
      const auto extent=std::max({frame.m_Max.x-frame.m_Min.x,frame.m_Max.y-frame.m_Min.y,frame.m_Max.z-frame.m_Min.z});
      if(!(extent>0))throw std::invalid_argument("VO diagnostic bounds");
      const float scale=1.8f/extent;
      std::vector<VectorMaterial> materials(model.Surfaces().size());
      for(auto& material:materials) {
        material.texture_slot=0;material.sampling=TextureSampling::NearestRepeat;
        auto& clip=material.draw.clip_from_model;clip[0]=clip[5]=scale;clip[10]=scale*.5f;clip[15]=1;
        clip[12]=-(frame.m_Min.x+frame.m_Max.x)*scale*.5f;
        clip[13]=-(frame.m_Min.y+frame.m_Max.y)*scale*.5f;
        clip[14]=.5f-(frame.m_Min.z+frame.m_Max.z)*scale*.25f;
        for(int i=0;i<4;++i)material.draw.normal_from_model[i*5]=1;
      }
      const auto submissions=PlanVectorFrame(model,0,materials);
      std::vector<std::byte> rgba(64*64*4);renderer.DrawFrame(submissions,rgba);
      std::size_t covered=0;
      for(std::size_t i=0;i<rgba.size();i+=4)
        if(rgba[i]!=std::byte{0}||rgba[i+1]!=std::byte{0}||rgba[i+2]!=std::byte{0})++covered;
      if(!covered)throw std::runtime_error("VO GPU readback empty");
      std::printf("VO GPU geometry-only frame=0 vertices=%zu groups=%zu covered=%zu; textures=false battle_executed=false\n",
          model.Vertices().size(),submissions.size(),covered);
      return 0;
    }
    if(argc!=1)throw std::invalid_argument("expected --geometry-only <standalone VO> or no arguments");
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
