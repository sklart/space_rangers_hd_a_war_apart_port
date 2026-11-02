// SPDX-License-Identifier: GPL-2.0-or-later
// Actual Windows HAL DrawPrimitiveUP/readback, single-sample target only.
#include <windows.h>
#include <d3d9.h>
#include <array>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <vector>
void Check(HRESULT r){assert(SUCCEEDED(r));}
int main(int argc,char** argv){
 assert(argc==2);auto window=CreateWindowExW(0,L"STATIC",L"MatrixGame single-sample reference",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);assert(window);
 auto* api=Direct3DCreate9(D3D_SDK_VERSION);assert(api);D3DPRESENT_PARAMETERS present{};
 present.Windowed=TRUE;present.SwapEffect=D3DSWAPEFFECT_DISCARD;present.hDeviceWindow=window;present.BackBufferWidth=present.BackBufferHeight=64;present.BackBufferFormat=D3DFMT_X8R8G8B8;
 IDirect3DDevice9* device{};Check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_FPU_PRESERVE,&present,&device));
 IDirect3DSurface9* target{};Check(device->GetRenderTarget(0,&target));D3DSURFACE_DESC desc{};Check(target->GetDesc(&desc));assert(desc.MultiSampleType==D3DMULTISAMPLE_NONE && !desc.MultiSampleQuality);
 IDirect3DSurface9* staging{};Check(device->CreateOffscreenPlainSurface(64,64,desc.Format,D3DPOOL_SYSTEMMEM,&staging,nullptr));
 Check(device->SetRenderState(D3DRS_ZENABLE,FALSE));Check(device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE));Check(device->SetRenderState(D3DRS_LIGHTING,FALSE));Check(device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE));
 Check(device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE));Check(device->SetTexture(0,nullptr));Check(device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1));Check(device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE));
 Check(device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1));Check(device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE));Check(device->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE));
 struct Vertex{float x,y,z,rhw;DWORD color;};const Vertex vertices[]={{3.25f,5.5f,.5f,1,0xffff0000},{57.75f,13.25f,.5f,1,0xff00ff00},{19.5f,60.75f,.5f,1,0xff0000ff}};
 std::vector<char> baseline;std::ofstream output(argv[1],std::ios::binary);assert(output);unsigned cases=0;
 for(DWORD enabled:{0u,1u})for(DWORD mask:{0u,1u,0x55555555u,~0u}){
  Check(device->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS,enabled));Check(device->SetRenderState(D3DRS_MULTISAMPLEMASK,mask));DWORD actual{};
  Check(device->GetRenderState(D3DRS_MULTISAMPLEANTIALIAS,&actual));assert(actual==enabled);Check(device->GetRenderState(D3DRS_MULTISAMPLEMASK,&actual));assert(actual==mask);
  Check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff172b51,1,0));Check(device->BeginScene());Check(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,vertices,sizeof(Vertex)));Check(device->EndScene());
  Check(device->GetRenderTargetData(target,staging));D3DLOCKED_RECT lock{};Check(staging->LockRect(&lock,nullptr,D3DLOCK_READONLY));std::vector<char> image(64*64*4);
  for(unsigned y=0;y<64;++y)std::memcpy(image.data()+y*256,static_cast<char*>(lock.pBits)+std::size_t(y)*lock.Pitch,256);Check(staging->UnlockRect());
  if(baseline.empty())baseline=image;else assert(baseline==image);output.write(image.data(),image.size());++cases;
 }
 unsigned covered=0;for(unsigned i=0;i<64*64;++i)if(static_cast<unsigned char>(baseline[i*4])!=0x51 || static_cast<unsigned char>(baseline[i*4+1])!=0x2b || static_cast<unsigned char>(baseline[i*4+2])!=0x17)++covered;
 assert(covered>500 && covered<3000 && output.good());staging->Release();target->Release();device->Release();api->Release();DestroyWindow(window);
 std::printf("WINDOWS HAL cases=%u single_sample=1 triangle_pixels=%u TRUE_FALSE_masks=4 GetRenderState_exact=PASS all_Draw_readback_bytes_equal=PASS NO_PRESENT NO_NATIVE_INIT\n",cases,covered);
}
