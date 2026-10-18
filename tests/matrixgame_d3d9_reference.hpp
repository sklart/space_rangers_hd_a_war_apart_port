// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Actual Windows D3D9 draw/readback helpers; no emulated graphics API.
#include <windows.h>
#include <d3d9.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
namespace srhd_awa::matrixgame::d3d9_reference {
template<class T> struct Owner {
  T* value{};
  ~Owner(){if(value)value->Release();}
  T** put(){return &value;}
  T* operator->()const{return value;}
};
inline void Check(HRESULT result,const char* operation) {
  if(FAILED(result)) {char text[160];std::snprintf(text,sizeof(text),"%s: %08lx",operation,unsigned long(result));throw std::runtime_error(text);}
}
struct Window {
  HWND value{CreateWindowExW(0,L"STATIC",L"MatrixGame sampler reference",WS_POPUP,0,0,16,16,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
  ~Window(){if(value)DestroyWindow(value);}
};
constexpr unsigned Width=64,Height=64;
using Pixels=std::array<std::uint32_t,Width*Height>;
inline Pixels Render(IDirect3DDevice9* device,IDirect3DTexture9* texture,float extent,float u0=0,float v0=0) {
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
