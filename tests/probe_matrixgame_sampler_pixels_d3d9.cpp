// SPDX-License-Identifier: GPL-2.0-or-later
// Real D3D9 min/mag/mip, LOD bias and address-mode pixel reference.
#include "../port/matrixgame/deko/sampler_state.hpp"
#include "matrixgame_d3d9_reference.hpp"
#include <string>
using namespace srhd_awa::matrixgame;
using namespace srhd_awa::matrixgame::d3d9_reference;
namespace {
Pixels Render(IDirect3DDevice9* device,IDirect3DTexture9* texture,const MeshSamplerState& state,float extent,float u=0,float v=0) {
  state.Validate();const auto words=state.Words();
  for(std::size_t i=0;i<SamplerRegisters.size();++i)
    Check(device->SetSamplerState(0,static_cast<D3DSAMPLERSTATETYPE>(SamplerRegisters[i]),words[i]),"SetSamplerState");
  return d3d9_reference::Render(device,texture,extent,u,v);
}
}

int main(int argc,char**argv) {
  if(argc!=2||(std::string(argv[1])!="--hal"&&std::string(argv[1])!="--ref"))return 2;
  const bool hardware=std::string(argv[1])=="--hal";
  try {
    Window window;if(!window.value)return 77;
    Owner<IDirect3D9> api;api.value=Direct3DCreate9(D3D_SDK_VERSION);if(!api.value)return 77;
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window.value;
    pp.BackBufferWidth=Width;pp.BackBufferHeight=Height;pp.BackBufferFormat=D3DFMT_UNKNOWN;
    pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    Owner<IDirect3DDevice9> device;
    const auto result=api->CreateDevice(0,hardware?D3DDEVTYPE_HAL:D3DDEVTYPE_REF,window.value,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.put());
    if(FAILED(result)){std::fprintf(stderr,"CreateDevice %s unavailable: %08lx\n",hardware?"HAL":"REF",unsigned long(result));return 77;}
    Owner<IDirect3DTexture9> texture,pattern;
    Check(device->CreateTexture(64,64,7,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,texture.put(),nullptr),"CreateMips");
    for(unsigned level=0;level<7;++level) {
      D3DLOCKED_RECT lock{};Check(texture->LockRect(level,&lock,nullptr,0),"LockMip");
      for(unsigned y=0;y<(64u>>level);++y)for(unsigned x=0;x<(64u>>level);++x)
        reinterpret_cast<std::uint32_t*>(static_cast<char*>(lock.pBits)+std::size_t(y)*lock.Pitch)[x]=0xff000000u|((32+32*level)<<16);
      Check(texture->UnlockRect(level),"UnlockMip");
    }
    Check(device->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,pattern.put(),nullptr),"CreatePattern");
    D3DLOCKED_RECT lock{};Check(pattern->LockRect(0,&lock,nullptr,0),"LockPattern");
    const std::array<std::uint32_t,4> colors{0xff100000u,0xff500000u,0xff900000u,0xffd00000u};
    for(unsigned y=0;y<2;++y)std::memcpy(static_cast<char*>(lock.pBits)+y*lock.Pitch,colors.data()+2*y,8);
    Check(pattern->UnlockRect(0),"UnlockPattern");
    unsigned controls=0,wrong=0;
    auto check=[&](const char* name,const Pixels& pixels,unsigned red,unsigned tolerance=0) {
      unsigned failed=0;
      // Avoid triangle edges. Interior is constant for every control below.
      for(unsigned y=8;y<56;++y)for(unsigned x=8;x<56;++x) {
        const auto p=pixels[y*Width+x];const auto r=(p>>16)&255;
        failed+=((p&0xff00ffffu)!=0xff000000u||r+tolerance<red||r>red+tolerance);
      }
      ++controls;wrong+=failed;
      std::fprintf(stderr,"%s expected_red=%u tolerance=%u first=%08x bad=%u\n",name,red,tolerance,pixels[32*Width+32],failed);
    };
    MeshSamplerState state;state.mip=TextureMipFilter::Linear;state.lod_bias=.25f;
    check("point texels + linear mip",Render(device.value,texture.value,state,2),72,1);
    state.mip=TextureMipFilter::Point;
    check("point mip",Render(device.value,texture.value,state,2),64);
    state.mip=TextureMipFilter::None;
    check("no mip",Render(device.value,texture.value,state,2),32);
    state.mip=TextureMipFilter::Linear;state.lod_bias=-1;
    check("bias -1",Render(device.value,texture.value,state,2),32);
    state.lod_bias=1;
    check("bias +1",Render(device.value,texture.value,state,2),96);
    state=MeshSamplerState{};
    check("repeat UV",Render(device.value,pattern.value,state,0,1.25f,-.25f),144);
    state.u=TextureAddress::Clamp;
    check("clamp U repeat V",Render(device.value,pattern.value,state,0,1.25f,-.25f),208);
    state.v=TextureAddress::Clamp;
    check("clamp UV",Render(device.value,pattern.value,state,0,1.25f,-.25f),80);
    state.u=TextureAddress::Repeat;
    check("repeat U clamp V",Render(device.value,pattern.value,state,0,1.25f,-.25f),16);
    state.u=state.v=TextureAddress::Clamp;state.mag=TextureFilter::Linear;
    check("linear mag point min",Render(device.value,pattern.value,state,0,.5f,.5f),112,1);
    state.mag=TextureFilter::Point;state.min=TextureFilter::Linear;
    check("point mag linear min",Render(device.value,pattern.value,state,0,.5f,.5f),208);
    state=MeshSamplerState{};state.mag=TextureFilter::Linear;
    check("point min linear mag",Render(device.value,pattern.value,state,128),16);
    state.mag=TextureFilter::Point;state.min=TextureFilter::Linear;
    check("linear min point mag",Render(device.value,pattern.value,state,128),112,1);
    state=MeshSamplerState{};state.min=TextureFilter::Linear;state.mag=TextureFilter::Linear;
    state.mip=TextureMipFilter::Linear;state.lod_bias=.25f;state.max_anisotropy=16;
    check("linear filters ignore persistent max AF",Render(device.value,texture.value,state,2),72,1);
    D3DADAPTER_IDENTIFIER9 adapter{};Check(api->GetAdapterIdentifier(0,0,&adapter),"AdapterIdentifier");
    std::printf("{\"device\":\"%s\",\"adapter_vendor\":%u,\"adapter_device\":%u,\"controls\":%u,\"checked_pixels_per_control\":2304,\"unexpected_pixels\":%u,\"draw_executed\":true,\"switch_gpu_executed\":false}\n",
      hardware?"HAL":"REF",adapter.VendorId,adapter.DeviceId,controls,wrong);
    return wrong?3:0;
  }catch(const std::exception& error){std::fprintf(stderr,"sampler reference: %s\n",error.what());return 3;}
}
