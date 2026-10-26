#include <d3dx9tex.h>
// Exact original D3DX font bake and independent native-layout HAL oracle.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9core.h>
#include "native_debug_font.hpp"
#include <array>
#include <fstream>
#include <map>
#include <set>
#include <cstdio>
#include <cassert>
#include <cstring>
namespace f=srhd_awa::matrixgame::debug_font::native;
void Check(HRESULT value){if(FAILED(value))throw std::runtime_error("real D3D9/GDI font operation failed: "+std::to_string(unsigned(value)));}
template<class T>struct Com {T* p{};~Com(){if(p)p->Release();}T* operator->()const{return p;}};
struct Device {
    HWND window{};Com<IDirect3D9> api;Com<IDirect3DDevice9> device;Com<ID3DXFont> font;
    Device(){window=CreateWindowW(L"STATIC",L"MatrixGame original font oracle",WS_OVERLAPPEDWINDOW,0,0,1024,768,nullptr,nullptr,GetModuleHandle(nullptr),nullptr);
        api.p=Direct3DCreate9(D3D_SDK_VERSION);D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;
        Check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&device.p));
        Check(D3DXCreateFontW(device.p,25,0,FW_NORMAL,1,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,PROOF_QUALITY,DEFAULT_PITCH,L"MS Sans Serif",&font.p));}
    ~Device(){if(window)DestroyWindow(window);}
};
struct Target {
    IDirect3DDevice9* device;Com<IDirect3DSurface9> surface,readback;
    unsigned w,h;
    Target(IDirect3DDevice9* d,unsigned width,unsigned height):device(d),w(width),h(height){
        Check(d->CreateRenderTarget(w,h,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&surface.p,nullptr));
        Check(d->CreateOffscreenPlainSurface(w,h,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback.p,nullptr));}
    void Begin(){Check(device->SetRenderTarget(0,surface.p));Check(device->SetDepthStencilSurface(nullptr));
        D3DVIEWPORT9 viewport{0,0,w,h,0,1};Check(device->SetViewport(&viewport));Check(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0));Check(device->BeginScene());}
    std::vector<std::byte> End(){Check(device->EndScene());Check(device->GetRenderTargetData(surface.p,readback.p));D3DLOCKED_RECT lock{};Check(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY));
        std::vector<std::byte> bytes(std::size_t(w)*h*4);for(unsigned y=0;y<h;++y)std::memcpy(bytes.data()+std::size_t(y)*w*4,static_cast<std::byte*>(lock.pBits)+std::size_t(y)*lock.Pitch,w*4);Check(readback->UnlockRect());return bytes;}
};
void Write(const char* path,std::span<const std::byte> bytes){std::ofstream stream(path,std::ios::binary);stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!stream)throw std::runtime_error("font artifact write");}

int main(){try{
 Device owner;D3DDISPLAYMODE mode{};Check(owner.api->GetAdapterDisplayMode(0,&mode));
 for(unsigned requested:{28u,51u,21u}){
  const auto raw=owner.api->CheckDeviceFormat(0,D3DDEVTYPE_HAL,mode.Format,D3DUSAGE_RENDERTARGET,D3DRTYPE_TEXTURE,D3DFORMAT(requested));
  Com<IDirect3DTexture9> texture;auto result=D3DXCreateTexture(owner.device.p,128,128,1,D3DUSAGE_RENDERTARGET,D3DFORMAT(requested),D3DPOOL_DEFAULT,&texture.p);Check(result);
  D3DSURFACE_DESC desc{};Check(texture->GetLevelDesc(0,&desc));assert(desc.Width==128 && desc.Height==128 && desc.Usage==D3DUSAGE_RENDERTARGET && desc.Pool==D3DPOOL_DEFAULT);
  std::printf("requested=%u raw_check=%08x D3DXCreateTexture=%08x actual_format=%u levels=%u dimensions=%ux%u\n",requested,unsigned(raw),unsigned(result),unsigned(desc.Format),texture->GetLevelCount(),desc.Width,desc.Height);
 }
 std::puts("original shadow D3DX format negotiation observed on Windows HAL; NO_SWITCH_GPU");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 10;}}
