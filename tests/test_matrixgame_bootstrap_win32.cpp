// Actual SDK creation is isolated from the portable CPU D3DX math names.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#include <cstdio>
namespace p=srhd_awa::matrixgame::platform3g;
int RunWindowsBootstrap(int argc,char**argv,int(*run)(int,char**,void*,p::Window&,p::Device&)) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    std::puts("Windows SDK: creating borrowed host window/device");
    const auto module=GetModuleHandleW(nullptr);
    const auto hwnd=CreateWindowExW(0,L"STATIC",L"MatrixGame bootstrap",WS_POPUP,0,0,16,16,nullptr,nullptr,module,nullptr);
    if(!hwnd)return 77;
    struct WindowOwner {HWND h;~WindowOwner(){DestroyWindow(h);}} own_window{hwnd};
    auto* api=Direct3DCreate9(D3D_SDK_VERSION);if(!api)return 77;
    struct ApiOwner {IDirect3D9* p;~ApiOwner(){p->Release();}} own_api{api};
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=hwnd;
    pp.BackBufferWidth=16;pp.BackBufferHeight=16;
    IDirect3DDevice9* raw=nullptr;
    const auto hr=api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_NULLREF,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&raw);
    if(FAILED(hr)){std::fprintf(stderr,"D3D9 NULLREF unavailable hr=%08lx\n",hr);return 77;}
    std::puts("Windows SDK: actual D3D9 NULLREF created");
    struct DeviceOwner {IDirect3DDevice9*p;~DeviceOwner(){p->Release();}} own_device{raw};
    IDirect3DTexture9* texture=nullptr;
    if(FAILED(raw->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture,nullptr)))return 77;
    struct TextureOwner {IDirect3DTexture9*p;~TextureOwner(){p->Release();}} own_texture{texture};
    if(FAILED(raw->SetTexture(0,texture)))return 77; // Real borrowed binding must survive native ClearTexture/Deinit.
    p::Win32Window window(hwnd);p::D3D9Device device(raw);
    return run(argc,argv,module,window,device);
}
