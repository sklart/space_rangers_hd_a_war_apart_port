// SPDX-License-Identifier: GPL-2.0-or-later
// Original CBitmap serializer + CTexture method; CPU upload recorder, no GPU.
#include "Texture.hpp"
#include "native_texture.hpp"
#include "unmanaged_texture_backend.hpp"
#include <cassert>
#include <bit>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
namespace m=srhd_awa::matrixgame;namespace p=m::platform3g;namespace nt=m::native_texture;
struct Image final:nt::Image {std::vector<nt::Level> levels;explicit Image(const nt::Texture& t):levels(t.levels){}};
struct Device final:p::Device,nt::Provider {
 std::shared_ptr<nt::Session> session=std::make_shared<nt::Session>();unsigned calls{};bool fail{};
 Device(){session->upload=[this](const nt::Texture& t){++calls;if(fail)throw std::bad_alloc();return std::make_shared<Image>(t);};}
 ~Device(){session->Shutdown();}
 std::shared_ptr<nt::Session> TextureSession()override{return session;}
 p::DeviceCaps ReadCaps()override{throw p::Unsupported("bitmap CPU recorder has no GPU caps");}
 std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("bitmap CPU recorder has no GPU budget");}
 p::GammaRamp ReadGamma(unsigned)override{throw p::Unsupported("bitmap CPU recorder has no VI");}
 void RestoreGamma(unsigned,const p::GammaRamp&)noexcept override{std::terminate();}
};
std::vector<char> Read(const std::string& path){std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
unsigned Word(const std::vector<char>& bytes,std::size_t offset){assert(offset+4<=bytes.size());unsigned v;std::memcpy(&v,bytes.data()+offset,4);return v;}
int main(int argc,char** argv){
 assert(argc==3);std::ifstream manifest(argv[1]);assert(manifest);std::ofstream observed(argv[2],std::ios::binary);assert(observed);
 Device d;p::Current().device=&d;p::Current().ready=true;
 const auto registry=m::legacy_texture::RegistrySize();assert(nt::HandleCount()==0);
 unsigned cases=0,levels=0;std::size_t bytes=0,differing=0,exact=0;unsigned max_error=0;
 for(std::string line;std::getline(manifest,line);){
  std::istringstream row(line);std::string dds_path,oracle_path;unsigned requested,pattern;
  row>>requested>>pattern>>dds_path>>oracle_path;assert(row);
  auto input=Read(dds_path),oracle=Read(oracle_path);const auto w=Word(input,16),h=Word(input,12),ch=Word(input,88)/8;
  CBitmap bm;ch==4?bm.CreateRGBA(w,h):bm.CreateRGB(w,h);
  for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
   auto* pixel=bm.Data()+std::size_t(y)*bm.Pitch()+x*ch;
   const auto* src=input.data()+128+(std::size_t(y)*w+x)*ch;
   pixel[0]=static_cast<unsigned char>(src[2]);pixel[1]=static_cast<unsigned char>(src[1]);pixel[2]=static_cast<unsigned char>(src[0]);
   if(ch==4)pixel[3]=static_cast<unsigned char>(src[3]);
  }
  Base::CBuf serialized;bm.SaveInDDSUncompressed(serialized);
  assert(std::size_t(serialized.Len())==input.size() && !std::memcmp(serialized.Get(),input.data(),input.size()));
  IDirect3DTexture9* handle{};CTexture original;
  if(!requested){original.MipmapOff();original.LoadFromBitmap(bm);handle=original.DX();
   // Preserve original Unload() resetting the flag before level selection.
   assert(!(original.Flags()&TF_NOMIPMAP));}
  else assert(m::unmanaged_texture::DecodeBitmap(serialized.Get(),serialized.Len(),requested,&handle)==0);
  auto texture=nt::Find(handle);assert(texture->format==Word(oracle,12) && texture->pool==0 && texture->usage==0);
  assert(texture->levels.size()==Word(oracle,24));auto image=std::dynamic_pointer_cast<Image>(texture->image);assert(image);
  std::size_t offset=28;bool same=true;
  for(const auto& level:image->levels){
   assert(level.width==Word(oracle,offset) && level.height==Word(oracle,offset+4) && level.bytes.size()==Word(oracle,offset+8));offset+=12;
   for(std::size_t i=0;i<level.bytes.size();++i){const unsigned a=unsigned(level.bytes[i]),b=static_cast<unsigned char>(oracle[offset+i]);
    const unsigned error=a>b?a-b:b-a;max_error=std::max(max_error,error);assert(error<=1);
    if(error){++differing;same=false;}
    // Copy and constant inputs have no floating resampling ambiguity.
    if((std::has_single_bit(w)&&std::has_single_bit(h)) || pattern==1)assert(!error);
   }
   observed.write(reinterpret_cast<const char*>(level.bytes.data()),level.bytes.size());bytes+=level.bytes.size();offset+=level.bytes.size();++levels;
  }
  assert(offset==oracle.size());exact+=same;++cases;texture.reset();image.reset();
  if(requested)nt::Release(handle);else original.Unload();assert(nt::HandleCount()==0);
 }
 // Failed upload never publishes a token; the actual original method can retry.
 {CBitmap bm;bm.CreateRGBA(3,5);std::memset(bm.Data(),127,std::size_t(bm.Pitch())*5);CTexture texture;
  d.fail=true;bool failed=false;try{texture.LoadFromBitmap(bm);}catch(const p::DeviceError&){failed=true;}
  assert(failed && !texture.DX() && nt::HandleCount()==0);d.fail=false;texture.LoadFromBitmap(bm);assert(texture.DX());
  texture.LoadFromBitmap(bm);assert(nt::HandleCount()==1);
  Base::CBuf data;bm.SaveInDDSUncompressed(data);
  for(unsigned kind=0;kind<4;++kind){auto malformed=std::vector<char>(static_cast<char*>(data.Get()),static_cast<char*>(data.Get())+data.Len());
   if(kind==0)malformed.resize(127);if(kind==1)malformed.pop_back();if(kind==2)malformed[80]=0;if(kind==3)malformed[16]=0;
   IDirect3DTexture9* output{};const auto before=d.calls;bool rejected=false;
   try{m::unmanaged_texture::DecodeBitmap(malformed.data(),malformed.size(),0,&output);}catch(const std::invalid_argument&){rejected=true;}
   assert(rejected && !output && d.calls==before && nt::HandleCount()==1);
  }
 }
 assert(nt::HandleCount()==0 && m::legacy_texture::RegistrySize()==registry);
 p::Current().ready=false;p::Current().device=nullptr;
 std::printf("cases=%u levels=%u bytes=%zu exact_cases=%zu differing_channels=%zu max_error=%u original_CBitmap_CTexture_cleanup_OOM_retry=PASS CPU_UPLOAD_RECORDER_ONLY NO_NATIVE_INIT NO_SWITCH\n",cases,levels,bytes,exact,differing,max_error);
}
