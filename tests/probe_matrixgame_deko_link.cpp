// SPDX-License-Identifier: GPL-2.0-or-later
// Linkable future physical probe. Building it is NOT evidence of a GPU frame.
#include "mesh_renderer.hpp"
#include "vector_submission.hpp"
#include "png_texture.hpp"
#include "CBitmap.hpp"
#include "CFile.hpp"
#include "texture_request.hpp"
#include "matrixgame_raster_gpu_probe.hpp"
#include "matrixgame_sampler_gpu_probe.hpp"
#include "mesh_shaders.hpp"
#include "config_unicode.hpp"
#include <array>
#include <cstdio>
#include <vector>
using namespace srhd_awa::matrixgame;
int main(int argc,char** argv) {
  try {
    DekoMeshRenderer renderer(64,64,std::as_bytes(std::span(vertex_dksh)),std::as_bytes(std::span(fragment_dksh)));
    if(argc==2 && std::string_view(argv[1])=="--sampler-state") {
      ProbeSamplerGpu(renderer);std::printf("GPU sampler reference PASS controls=15; battle_executed=false\n");return 0;
    }
    if(argc==2 && std::string_view(argv[1])=="--raster-state") {
      ProbeRasterGpu(renderer);std::printf("GPU raster state reference PASS controls=8; battle_executed=false\n");return 0;
    }
    if(argc==3 && std::string_view(argv[1])=="--texture-request") {
      const auto request=TextureRequest::Resolve(::matrixgame::native::ConfigWide(argv[2]));
      const auto asset=request.Load();renderer.SetTextureAsset(1,asset);
      std::printf("Texture request upload completed flags=%u supplied_mips=%zu; draw_executed=false; battle_executed=false\n",request.flags,asset.levels.size());return 0;
    }
    if(argc==4 && std::string_view(argv[1])=="--texture") {
      const auto asset=TextureAsset::Load(::matrixgame::native::ConfigWide(argv[2]));renderer.SetTextureAsset(1,asset);
      Base::CFile file{::matrixgame::native::ConfigWide(argv[3])};file.OpenRead();
      if(file.Size()!=64*64*4)throw std::invalid_argument("expected 64x64 RGBA texture reference");
      std::vector<std::byte> expected(64*64*4),rgba(expected.size());file.Read(expected.data(),expected.size());
      const std::array quad{
        MeshVertex{{-1,1,.5f},{0,0,1},{0,0}},MeshVertex{{1,1,.5f},{0,0,1},{1,0}},
        MeshVertex{{-1,-1,.5f},{0,0,1},{0,1}},MeshVertex{{1,-1,.5f},{0,0,1},{1,1}}};
      const std::array<std::uint16_t,6> indices{0,1,2,2,1,3};renderer.SetMesh(quad,indices);
      MeshSubmission submission{};submission.range={0,0,0,6,0,4};submission.texture_slot=1;submission.sampling=TextureSampling::NearestClamp;
      for(int i=0;i<4;++i)submission.draw.clip_from_model[i*5]=submission.draw.normal_from_model[i*5]=1;
      submission.draw.light[3]=1;renderer.DrawFrame(std::span(&submission,1),rgba);
      if(rgba!=expected)throw std::runtime_error("texture GPU base-level readback mismatch");
      std::printf("Texture GPU base-level reference PASS supplied_mips=%zu; battle_executed=false\n",asset.levels.size());return 0;
    }
    if(argc==3 && std::string_view(argv[1])=="--png") {
      const auto path=::matrixgame::native::ConfigWide(argv[2]);
      const auto size=UploadPngTexture(renderer,1,path);
      CBitmap reference;if(!reference.LoadRGBAFromPNG(path))throw std::runtime_error("PNG reference decode");
      const std::array quad{
        MeshVertex{{-1,1,.5f},{0,0,1},{0,0}},MeshVertex{{1,1,.5f},{0,0,1},{1,0}},
        MeshVertex{{-1,-1,.5f},{0,0,1},{0,1}},MeshVertex{{1,-1,.5f},{0,0,1},{1,1}}};
      const std::array<std::uint16_t,6> indices{0,1,2,2,1,3};renderer.SetMesh(quad,indices);
      MeshSubmission submission{};submission.range={0,0,0,6,0,4};submission.texture_slot=1;submission.sampling=TextureSampling::NearestClamp;
      for(int i=0;i<4;++i)submission.draw.clip_from_model[i*5]=submission.draw.normal_from_model[i*5]=1;
      submission.draw.light[3]=1;
      std::vector<std::byte> rgba(64*64*4);renderer.DrawFrame(std::span(&submission,1),rgba);
      for(std::size_t y=0;y<64;++y)for(std::size_t x=0;x<64;++x) {
        const auto sx=std::min<std::size_t>((2*x+1)*size.width/128,size.width-1);
        const auto sy=std::min<std::size_t>((2*y+1)*size.height/128,size.height-1);
        if(std::memcmp(rgba.data()+(y*64+x)*4,reference.Data()+sy*reference.Pitch()+sx*4,4))
          throw std::runtime_error("PNG GPU nearest readback mismatch");
      }
      std::printf("PNG GPU nearest readback PASS source=%ux%u output=64x64; battle_executed=false\n",size.width,size.height);return 0;
    }
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
    if(argc!=1)throw std::invalid_argument("expected --sampler-state, --raster-state, --texture-request <name>, --texture <source> <64x64.rgba>, --png <PNG>, --geometry-only <VO>, or no arguments");
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
