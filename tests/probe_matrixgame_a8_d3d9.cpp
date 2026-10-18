// SPDX-License-Identifier: GPL-2.0-or-later
// Real D3D9 draw/readback: A8 sampling versus its native RGBA8 representation.
// This is an optional Windows reference, never a NULLREF or Switch GPU proof.
#include <windows.h>
#include <d3d9.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <float.h>
#include <stdexcept>
#include <string>

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
  HWND value{CreateWindowExW(0,L"STATIC",L"MatrixGame A8 reference",WS_POPUP,0,0,16,16,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};
  ~Window(){if(value)DestroyWindow(value);}
};
constexpr unsigned Width=256,Height=4;
using Pixels=std::array<std::uint32_t,Width*Height>;
Pixels Render(IDirect3DDevice9* device,IDirect3DTexture9* texture) {
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
  for(auto key:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER})Check(device->SetSamplerState(0,key,D3DTEXF_POINT),"PointSampler");
  Check(device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE),"DisableMipFilter");
  Check(device->SetSamplerState(0,D3DSAMP_SRGBTEXTURE,FALSE),"DisableSrgb");
  for(auto key:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV})Check(device->SetSamplerState(0,key,D3DTADDRESS_CLAMP),"ClampSampler");
  struct Vertex{float x,y,z,rhw,u,v;};
  const std::array<Vertex,4> vertices{{{-.5f,-.5f,0,1,0,.5f},{Width-.5f,-.5f,0,1,1,.5f},
    {-.5f,Height-.5f,0,1,0,.5f},{Width-.5f,Height-.5f,0,1,1,.5f}}};
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
    const auto before=_controlfp(0,0);Owner<IDirect3DDevice9> device;
    const auto result=api->CreateDevice(0,hardware?D3DDEVTYPE_HAL:D3DDEVTYPE_REF,window.value,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,device.put());
    if(FAILED(result)){std::fprintf(stderr,"CreateDevice %s unavailable: %08lx\n",hardware?"HAL":"REF",unsigned long(result));return 77;}
    const auto after=_controlfp(0,0);
    Owner<IDirect3DTexture9> alpha,rgba;
    Check(device->CreateTexture(Width,1,1,0,D3DFMT_A8,D3DPOOL_MANAGED,alpha.put(),nullptr),"CreateA8");
    Check(device->CreateTexture(Width,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,rgba.put(),nullptr),"CreateRGBA8");
    D3DLOCKED_RECT locked{};Check(alpha->LockRect(0,&locked,nullptr,0),"LockA8");
    for(unsigned x=0;x<Width;++x)static_cast<unsigned char*>(locked.pBits)[x]=static_cast<unsigned char>(x);
    Check(alpha->UnlockRect(0),"UnlockA8");Check(rgba->LockRect(0,&locked,nullptr,0),"LockRGBA8");
    for(unsigned x=0;x<Width;++x)static_cast<std::uint32_t*>(locked.pBits)[x]=(x<<24);
    Check(rgba->UnlockRect(0),"UnlockRGBA8");
    const auto a=Render(device.value,alpha.value),b=Render(device.value,rgba.value);unsigned differences=0,wrong=0,wrong_rgba=0;
    for(unsigned y=0;y<Height;++y)for(unsigned x=0;x<Width;++x){const auto i=y*Width+x;differences+=a[i]!=b[i];wrong+=a[i]!=(x<<24);wrong_rgba+=b[i]!=(x<<24);}
    D3DADAPTER_IDENTIFIER9 adapter{};Check(api->GetAdapterIdentifier(0,0,&adapter),"AdapterIdentifier");
    std::printf("{\"device\":\"%s\",\"adapter_vendor\":%u,\"adapter_device\":%u,\"alpha_values\":256,\"pixels_per_texture\":1024,\"different_pixels\":%u,\"unexpected_a8_pixels\":%u,\"unexpected_rgba_pixels\":%u,\"first_a8\":%u,\"last_a8\":%u,\"first_rgba\":%u,\"last_rgba\":%u,\"fpu_before\":%u,\"fpu_after\":%u}\n",
      hardware?"HAL":"REF",adapter.VendorId,adapter.DeviceId,differences,wrong,wrong_rgba,a[0],a[255],b[0],b[255],before,after);
    return differences||wrong||wrong_rgba?3:0;
  }catch(const std::exception& error){std::fprintf(stderr,"A8 reference: %s\n",error.what());return 3;}
}
