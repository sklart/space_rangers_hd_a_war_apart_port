// Actual SDK creation is isolated from the portable CPU D3DX math names.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#include <cstdio>
#include <cassert>
namespace p=srhd_awa::matrixgame::platform3g;
static_assert(static_cast<unsigned>(p::PixelFormat::Argb8)==D3DFMT_A8R8G8B8 && static_cast<unsigned>(p::PixelFormat::Xrgb8)==D3DFMT_X8R8G8B8);
static_assert(static_cast<unsigned>(p::PixelFormat::Rgb565)==D3DFMT_R5G6B5 && static_cast<unsigned>(p::PixelFormat::Depth16)==D3DFMT_D16);
static_assert(static_cast<unsigned>(p::PixelFormat::Depth24Stencil8)==D3DFMT_D24S8 && static_cast<unsigned>(p::PixelFormat::Depth24)==D3DFMT_D24X8);
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
    D3DDEVICE_CREATION_PARAMETERS direct_creation{};
    assert(SUCCEEDED(raw->GetCreationParameters(&direct_creation)));
    const auto creation=device.ReadCreationParameters();
    std::printf("direct SDK creation: adapter=%u type=%u flags=%lu focus=%p expected=%p\n",direct_creation.AdapterOrdinal,direct_creation.DeviceType,direct_creation.BehaviorFlags,direct_creation.hFocusWindow,hwnd);
    assert(creation.AdapterOrdinal==direct_creation.AdapterOrdinal && creation.DeviceType==direct_creation.DeviceType && creation.BehaviorFlags==direct_creation.BehaviorFlags && creation.FocusWindow==direct_creation.hFocusWindow);
    D3DDISPLAYMODE direct_mode{};assert(SUCCEEDED(api->GetAdapterDisplayMode(creation.AdapterOrdinal,&direct_mode)));
    const auto mode=device.ReadAdapterDisplayMode(creation.AdapterOrdinal);
    assert(mode.Width==direct_mode.Width && mode.Height==direct_mode.Height && mode.RefreshRate==direct_mode.RefreshRate && static_cast<std::uint32_t>(mode.Format)==direct_mode.Format);
    RECT direct_client{},direct_outer{};
    assert(GetClientRect(hwnd,&direct_client) && GetWindowRect(hwnd,&direct_outer));
    assert(window.ReadClientRect()==(p::WindowRect{direct_client.left,direct_client.top,direct_client.right,direct_client.bottom}));
    assert(window.ReadWindowRect()==(p::WindowRect{direct_outer.left,direct_outer.top,direct_outer.right,direct_outer.bottom}));
    const auto raw_multi=api->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_A8R8G8B8,TRUE,D3DMULTISAMPLE_NONE,nullptr);
    assert(device.CheckMultisample(0,1,p::PixelFormat::Argb8,true,0)==static_cast<p::DeviceResult>(raw_multi));
    bool invalid_adapter=false;
    try{device.ReadAdapterDisplayMode(0xffffffffu);}catch(const p::DeviceError& e){invalid_adapter=e.result==static_cast<p::DeviceResult>(D3DERR_INVALIDCALL);}
    assert(invalid_adapter);
    const auto saved_cursor=GetCursor();const auto arrow=LoadCursorA(nullptr,IDC_ARROW);
    if(!arrow)return 77;
    SetCursor(arrow);
    struct CursorOwner {HCURSOR old;~CursorOwner(){SetCursor(old);}} restore_cursor{saved_cursor};
    return run(argc,argv,module,window,device);
}
