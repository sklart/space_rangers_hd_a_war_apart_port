// Actual PC HAL readback across Reset; no Switch GPU evidence.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "transition_backend.hpp"
#include <cassert>
#include <cstdio>
using namespace srhd_awa::matrixgame;
template<class T>struct Com {T* p{};~Com(){if(p)p->Release();}};
void Check(HRESULT h){if(FAILED(h))throw std::runtime_error("actual HAL operation failed: "+std::to_string(h));}
struct Readback final:native_form::ScreenshotProvider {
 IDirect3DDevice9* device{};unsigned calls{};
 native_form::ScreenshotPixels CaptureScreenshot()override{
  ++calls;Com<IDirect3DSurface9> source,copy;Check(device->GetRenderTarget(0,&source.p));
  D3DSURFACE_DESC desc{};Check(source.p->GetDesc(&desc));assert(desc.Format==D3DFMT_X8R8G8B8 || desc.Format==D3DFMT_A8R8G8B8);
  Check(device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy.p,nullptr));
  Check(device->GetRenderTargetData(source.p,copy.p));
  native_form::ScreenshotPixels image{desc.Width,desc.Height,desc.Width*4,native_form::ScreenshotFormat::Rgba8,{}};image.bytes.resize(image.pitch*image.height);
  D3DLOCKED_RECT locked{};Check(copy.p->LockRect(&locked,nullptr,D3DLOCK_READONLY));
  for(unsigned y=0;y<desc.Height;++y)for(unsigned x=0;x<desc.Width;++x){
   const auto* input=static_cast<const unsigned char*>(locked.pBits)+y*locked.Pitch+x*4;
   auto* output=image.bytes.data()+y*image.pitch+x*4;
   output[0]=std::byte(input[2]);output[1]=std::byte(input[1]);output[2]=std::byte(input[0]);output[3]=std::byte{255};
  }
  Check(copy.p->UnlockRect());return image;
 }
};
int main(){
 HWND window=CreateWindowExW(0,L"STATIC",L"MatrixGame capture HAL check",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);assert(window);
 {
  Com<IDirect3D9> api;api.p=Direct3DCreate9(D3D_SDK_VERSION);assert(api.p);Com<IDirect3DDevice9> device;
  D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.hDeviceWindow=window;pp.SwapEffect=D3DSWAPEFFECT_COPY;pp.BackBufferFormat=D3DFMT_X8R8G8B8;pp.BackBufferWidth=64;pp.BackBufferHeight=48;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
  Check(api.p->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device.p));
  Readback readback;readback.device=device.p;
  for(unsigned cycle=0;cycle<2;++cycle){
   pp.BackBufferWidth=64;pp.BackBufferHeight=48;Check(device.p->Reset(&pp));
   Check(device.p->Clear(0,nullptr,D3DCLEAR_TARGET,D3DCOLOR_XRGB(17,34,51),1,0));
   D3DRECT region{8,7,40,31};Check(device.p->Clear(1,&region,D3DCLEAR_TARGET,D3DCOLOR_XRGB(170,187,204),1,0));Check(device.p->Present(nullptr,nullptr,nullptr,nullptr));
   transition::LoadingCaptureScope capture(readback);
   // All readback COM owners have gone. Reset and overwrite the actual target.
   pp.BackBufferWidth=80;pp.BackBufferHeight=60;Check(device.p->Reset(&pp));
   Check(device.p->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0));Check(device.p->Present(nullptr,nullptr,nullptr,nullptr));
   transition::Bitmap bitmap;transition::CaptureLoadingImage(bitmap,64,48);
   assert(readback.calls==cycle+1 && bitmap.SizeX()==64 && bitmap.SizeY()==48);
   for(unsigned y=0;y<48;++y)for(unsigned x=0;x<64;++x)for(unsigned c=0;c<3;++c){
    const bool inside=x>=8 && x<40 && y>=7 && y<31;const unsigned expected=(inside?170:17)+17*c;
    assert(bitmap.Data()[y*bitmap.Pitch()+x*3+c]==expected);
   }
   bool rejected=false;try{transition::CaptureLoadingImage(bitmap,64,48);}catch(const std::logic_error&){rejected=true;}assert(rejected);
  }
 }
 assert(DestroyWindow(window));
 std::puts("LOADING_CAPTURE_WINDOWS_HAL actual_completed_readback=PASS reset_repaint_owned_pixels=PASS rgb_oracle=18432 two_cycles=PASS one_shot=PASS COM_cleanup=PASS NO_SWITCH");
}
