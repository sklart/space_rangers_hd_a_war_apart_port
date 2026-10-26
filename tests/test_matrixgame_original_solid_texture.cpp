// Real original CTexture::MakeSolidTexture, including its staging owner.
#include "Texture.hpp"
#include "native_texture.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
namespace m=srhd_awa::matrixgame;namespace p=m::platform3g;namespace nt=m::native_texture;
struct Image final:nt::Image {std::vector<nt::Level> levels;explicit Image(const nt::Texture& t):levels(t.levels){}};
struct Device final:p::Device,nt::Provider{
 std::shared_ptr<nt::Session> session=std::make_shared<nt::Session>();unsigned calls{},fail{};
 Device(){session->upload=[this](const nt::Texture& t){if(++calls==fail)throw std::bad_alloc();return std::make_shared<Image>(t);};}
 ~Device(){session->Shutdown();}
 std::shared_ptr<nt::Session> TextureSession()override{return session;}
 p::DeviceCaps ReadCaps()override{throw p::Unsupported("solid CPU model has no GPU caps");}
 std::uint64_t AvailableTextureBytes()override{throw p::Unsupported("solid CPU model has no GPU budget");}
 p::GammaRamp ReadGamma(std::uint32_t)override{throw p::Unsupported("solid CPU model has no VI");}
 void RestoreGamma(std::uint32_t,const p::GammaRamp&)noexcept override{std::terminate();}
};
int main(){Device d;p::Current().ready=true;p::Current().device=&d;auto count=m::legacy_texture::RegistrySize();assert(nt::HandleCount()==0);
 for(unsigned fail:{0u,1u,2u}){
  {CTexture texture;d.calls=0;d.fail=fail;bool failed=false;try{texture.MakeSolidTexture(2,2,0xa1b2c3d4);}catch(const p::DeviceError&){failed=true;}assert(failed==bool(fail));
   if(!fail){auto owner=nt::Find(texture.DX());assert(owner->pool==0 && owner->levels.size()==1);auto image=std::dynamic_pointer_cast<Image>(owner->image);for(unsigned i=0;i<4;++i){unsigned value;std::memcpy(&value,image->levels[0].bytes.data()+4*i,4);assert(value==0xa1b2c3d4);}}
   else{assert(nt::HandleCount()==(fail==2?1u:0u));d.calls=0;d.fail=0;texture.MakeSolidTexture(2,2,0x12345678);assert(nt::Find(texture.DX())->levels[0].bytes[0]==std::byte{0x78});}
  }assert(nt::HandleCount()==0 && m::legacy_texture::RegistrySize()==count);
 }
 p::Current().ready=false;p::Current().device=nullptr;std::puts("original CTexture::MakeSolidTexture exact 2x2 BGRA/default-pool/create-OOM/update-OOM/staging-release/retry/destructor PASS CPU_UPLOAD_MODEL_ONLY NO_NATIVE_INIT SWITCH_HARDWARE_NOT_RUN");
}
