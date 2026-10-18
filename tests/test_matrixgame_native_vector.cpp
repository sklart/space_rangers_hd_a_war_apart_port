// SPDX-License-Identifier: GPL-2.0-or-later
// Actual CStorage/PKG -> owned VO -> draw ranges. No GPU/battle is simulated.
#include "vector_geometry.hpp"
#include "../port/matrixgame/deko/vector_submission.hpp"
#include "CFile.hpp"
#include "CException.hpp"
#include "config_unicode.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace srhd_awa::matrixgame;
namespace fs=std::filesystem;
struct Writer {
  std::ofstream stream;
  explicit Writer(const fs::path& p):stream(p,std::ios::binary) { if(!stream)throw std::runtime_error("VO dump open"); }
  void u32(std::uint32_t v){stream.write(reinterpret_cast<const char*>(&v),4);}
  template<class T> void blob(std::span<const T> data) {
    u32(static_cast<std::uint32_t>(data.size_bytes()));
    if(!data.empty())stream.write(reinterpret_cast<const char*>(data.data()),data.size_bytes());
  }
  void name(std::u16string_view s){blob<char16_t>(s);}
};
void Dump(const VectorGeometry& v,const fs::path& path) {
  Writer w(path);w.u32(0x31474f56);
  w.blob(v.Vertices());w.blob(v.Indices());w.blob(v.Frames());w.blob(v.FrameVertices());
  w.blob(v.FrameGroups());w.blob(v.Edges());w.blob(v.Groups());w.blob(v.AnimationFrames());w.blob(v.Matrices());
  w.u32(static_cast<std::uint32_t>(v.Surfaces().size()));for(const auto& n:v.Surfaces())w.name(n);
  w.u32(static_cast<std::uint32_t>(v.Properties().size()));for(const auto& p:v.Properties()){w.name(p.first);w.name(p.second);}
  w.u32(static_cast<std::uint32_t>(v.Animations().size()));
  for(const auto& a:v.Animations()){w.name(a.name);w.u32(a.id);w.u32(a.first);w.u32(a.count);}
  w.u32(static_cast<std::uint32_t>(v.MatrixNames().size()));
  for(const auto& m:v.MatrixNames()){w.name(m.name);w.u32(m.id);w.u32(m.first);}
  w.u32(v.Simple().has_value());
  if(v.Simple()) {
    w.blob(std::span(&v.Simple()->bounds,1));
    w.blob<vo::SVOTriangle>(v.Simple()->triangles);w.blob<D3DXVECTOR3>(v.Simple()->vertices);
  }
  // Explicit material fixtures test forwarding and original surface-first order.
  std::vector<VectorMaterial> materials(v.Surfaces().size());
  for(std::size_t i=0;i<materials.size();++i){
    materials[i].texture_slot=std::uint32_t(i+1);materials[i].sampling=TextureSampling::LinearClamp;
    materials[i].sampling.min=(i&1)?TextureFilter::Anisotropic:TextureFilter::Point;
    materials[i].sampling.max_anisotropy=8;materials[i].sampling.mip=TextureMipFilter::Linear;
    materials[i].sampling.v=TextureAddress::Repeat;materials[i].sampling.lod_bias=-1;
    materials[i].raster.alpha_test=(i&1)!=0;materials[i].raster.blend_enable=(i&2)!=0;
    materials[i].raster.depth_write=(i&2)==0;materials[i].raster.alpha_reference=(i&1)?192:8;
  }
  w.u32(static_cast<std::uint32_t>(v.Frames().size()));
  for(std::uint32_t i=0;i<v.Frames().size();++i) {
    const auto submissions=PlanVectorFrame(v,i,materials);w.u32(static_cast<std::uint32_t>(submissions.size()));
    for(const auto& s:submissions) {
      w.u32(static_cast<std::uint32_t>(s.range.surface));w.u32(static_cast<std::uint32_t>(s.range.base_vertex));
      w.u32(s.range.first_index);w.u32(s.range.index_count);w.u32(s.range.minimum_vertex);w.u32(s.range.vertex_count);
      if(s.texture_slot!=std::uint32_t(s.range.surface+1)||s.sampling.Words()!=materials[s.range.surface].sampling.Words())
        throw std::runtime_error("VO material forwarding");
      if(s.raster.Words()!=materials[s.range.surface].raster.Words())throw std::runtime_error("VO raster state forwarding");
    }
  }
  bool rejected=false;try{v.FrameRanges(static_cast<std::uint32_t>(v.Frames().size()));}catch(const std::out_of_range&){rejected=true;}
  if(!rejected)throw std::runtime_error("VO frame bounds not checked");
  rejected=false;try{PlanVectorFrame(v,0,{});}catch(const std::invalid_argument&){rejected=true;}
  if(!materials.empty()&&!rejected)throw std::runtime_error("VO missing material accepted");
  if(!materials.empty()) {
    materials[0].sampling.min=static_cast<TextureFilter>(0);
    rejected=false;try{PlanVectorFrame(v,0,materials);}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)throw std::runtime_error("VO invalid sampler accepted");
  }
  if(!w.stream)throw std::runtime_error("VO dump write");
}
int main(int argc,char** argv) {
  if(argc!=4 && argc!=5)return 2;
  try {
    const fs::path input=fs::u8path(argv[1]),output=fs::u8path(argv[2]);
    fs::create_directories(output);std::ifstream manifest(fs::u8path(argv[3]));
    if(!manifest)return 2;
    CFile::StaticInit();
    if(argc==5){const auto p=::matrixgame::native::ConfigWide(argv[4]);CFile::AddPackFile(p.c_str());CFile::OpenPackFiles();}
    std::string entry;std::size_t index=0,accepted=0,rejected=0;
    while(std::getline(manifest,entry)) {
      if(entry.empty())continue;
      try {
        const auto name=argc==5 ? ::matrixgame::native::ConfigWide(entry) :
            ::matrixgame::native::ConfigWide((input/fs::u8path(entry)).generic_string());
        const auto model=VectorGeometry::Load(name.c_str());
        Dump(model,output/(std::to_string(index)+".native"));
        std::cout<<index<<"\tPASS\n";++accepted;
      } catch(const Base::CException&) {std::cout<<index<<"\tREJECT storage\n";++rejected;}
        catch(const std::exception& e) {std::cout<<index<<"\tREJECT "<<e.what()<<"\n";++rejected;}
      ++index;
    }
    CFile::ReleasePackFiles();
    std::cout<<"accepted="<<accepted<<" rejected="<<rejected<<" battle_executed=false\n";
    return 0;
  } catch(const Base::CException&) {return 2;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
