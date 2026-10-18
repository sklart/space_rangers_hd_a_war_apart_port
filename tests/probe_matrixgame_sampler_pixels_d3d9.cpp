// SPDX-License-Identifier: GPL-2.0-or-later
// Real D3D9 draw/readback: independent filters, LOD bias and U/V addressing.
// This is an optional Windows reference, never a NULLREF or Switch GPU proof.
#include "../port/matrixgame/deko/sampler_state.hpp"
#include <windows.h>
#include <d3d9.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <float.h>
#include <stdexcept>
#include <string>

using namespace srhd_awa::matrixgame;
namespace {
template<class T> struct Owner {
  T* value{};
  ~Owner(){if(value)value->Release();}
  T** put(){return &value;}
  T* operator->()const{return value;}
};
void Check(HRESULT result,const char* operation) {
  if(FAILED(result)) {char text[160];std::snprintf(text,sizeof(text),"%s: %08lx",operation,unsigned long(result));throw std::runtime_error(text);}
}
struct Window {
  HWND value{CreateWindowExW(0,L"STATIC",L"MatrixGame sampler reference",WS_POPUP,0,0,16,16,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
  ~Window(){if(value)DestroyWindow(value);}
};
constexpr unsigned Width=64,Height=64;
using Pixels=std::array<std::uint32_t,Width*Height>;
Pixels Render(IDirect3DDevice9* device,IDirect3DTexture9* texture,const MeshSamplerState& state,float extent,float u0=0,float v0=0) {
  Owner<IDirect3DSurface9> target,readback;
  Check(device->CreateRenderTarget(Width,Height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,target.put(),nullptr),"CreateRenderTarget");
  Check(device->CreateOffscreenPlainSurface(Width,Height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,readback.put(),nullptr),"CreateReadback");
  Check(device->SetRenderTarget(0,target.value),"SetRenderTarget");
  Check(device->SetDepthStencilSurface(nullptr),"SetDepthStencilSurface");
  D3DVIEWPORT9 viewport{0,0,Width,Height,0,1};Check(device->SetViewport(&viewport),"SetViewport");
  const std::array<std::pair<D3DRENDERSTATETYPE,DWORD>,12> states{{
    {D3DRS_ZENABLE,FALSE},{D3DRS_ALPHATESTENABLE,FALSE},{D3DRS_ALPHABLENDENABLE,FALSE},
    {D3DRS_STENCILENABLE,FALSE},{D3DRS_FOGENABLE,FALSE},{D3DRS_LIGHTING,FALSE},
    {D3DRS_CULLMODE,D3DCULL_NONE},{D3DRS_DITHERENABLE,FALSE},{D3DRS_SRGBWRITEENABLE,FALSE},
    {D3DRS_SCISSORTESTENABLE,FALSE},{D3DRS_COLORWRITEENABLE,15},{D3DRS_CLIPPING,TRUE}}};
  for(auto [key,value]:states)Check(device->SetRenderState(key,value),"SetRenderState");
  Check(device->SetVertexShader(nullptr),"SetVertexShader");Check(device->SetPixelShader(nullptr),"SetPixelShader");
  Check(device->SetTexture(0,texture),"SetTexture");
  for(auto key:{D3DTSS_COLOROP,D3DTSS_ALPHAOP})Check(device->SetTextureStageState(0,key,D3DTOP_SELECTARG1),"SelectTexture");
  for(auto key:{D3DTSS_COLORARG1,D3DTSS_ALPHAARG1})Check(device->SetTextureStageState(0,key,D3DTA_TEXTURE),"TextureArgument");
  Check(device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE),"DisableStage1");
  Check(device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0),"TextureCoordinates");
  Check(device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE),"DisableTextureTransform");
  state.Validate();const auto words=state.Words();
  for(std::size_t i=0;i<SamplerRegisters.size();++i)
    Check(device->SetSamplerState(0,static_cast<D3DSAMPLERSTATETYPE>(SamplerRegisters[i]),words[i]),"SetSamplerState");
  Check(device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE),"DisableSrgb");
  struct Vertex{float x,y,z,rhw,u,v;};
  const std::array<Vertex,4> vertices{{{-.5f,-.5f,0,1,u0,v0},{Width-.5f,-.5f,0,1,u0+extent,v0},
    {-.5f,Height-.5f,0,1,u0,v0+extent},{Width-.5f,Height-.5f,0,1,u0+extent,v0+extent}}};
  Check(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1),"SetFVF");
  Check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0x12345678,1,0),"Clear");
  Check(device->BeginScene(),"BeginScene");
  const auto draw=device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices.data(),sizeof(Vertex));
  const auto end=device->EndScene();Check(draw,"DrawPrimitiveUP");Check(end,"EndScene");
  Check(device->GetRenderTargetData(target.value,readback.value),"GetRenderTargetData");
  D3DLOCKED_RECT locked{};Check(readback->LockRect(&locked,nullptr,D3DLOCK_READONLY),"LockReadback");
  Pixels pixels{};for(unsigned y=0;y<Height;++y)std::memcpy(pixels.data()+y*Width,static_cast<const char*>(locked.pBits)+std::size_t(y)*locked.Pitch,Width*4);
  Check(readback->UnlockRect(),"UnlockReadback");return pixels;
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
    D3DADAPTER_IDENTIFIER9 adapter{};Check(api->GetAdapterIdentifier(0,0,&adapter),"AdapterIdentifier");
    std::printf("{\"device\":\"%s\",\"adapter_vendor\":%u,\"adapter_device\":%u,\"controls\":%u,\"checked_pixels_per_control\":2304,\"unexpected_pixels\":%u,\"draw_executed\":true,\"switch_gpu_executed\":false}\n",
      hardware?"HAL":"REF",adapter.VendorId,adapter.DeviceId,controls,wrong);
    return wrong?3:0;
  }catch(const std::exception& error){std::fprintf(stderr,"sampler reference: %s\n",error.what());return 3;}
}
