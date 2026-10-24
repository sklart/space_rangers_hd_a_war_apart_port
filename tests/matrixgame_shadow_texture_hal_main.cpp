#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#include <cstdio>
#include <filesystem>
namespace p=srhd_awa::matrixgame::platform3g;
void VerifyOriginalShadowTextureDevice(const wchar_t*,p::Device&,void(*)());
namespace {
IDirect3DDevice9* checked_device;IDirect3DVertexBuffer9* checked_vertex;IDirect3DIndexBuffer9* checked_index;int checked_calls;
void CheckBindings(){
    IDirect3DVertexBuffer9* vb=nullptr;IDirect3DIndexBuffer9* ib=nullptr;UINT offset{},stride{};
    p::CheckDeviceResult(checked_device->GetStreamSource(0,&vb,&offset,&stride),"shadow binding oracle stream");
    p::CheckDeviceResult(checked_device->GetIndices(&ib),"shadow binding oracle index");
    const bool same=vb==checked_vertex && ib==checked_index && offset==4 && stride==12;
    if(vb)vb->Release();if(ib)ib->Release();if(!same)throw std::runtime_error("shadow SDK bindings changed");++checked_calls;
}
}
int main(int argc,char**argv){
    std::setvbuf(stdout,nullptr,_IONBF,0);if(argc!=2)return 2;
    const auto module=GetModuleHandleW(nullptr);
    auto hwnd=CreateWindowExW(0,L"STATIC",L"MatrixGame shadow GPU diagnostic",WS_POPUP,0,0,16,16,nullptr,nullptr,module,nullptr);
    if(!hwnd)return 77;struct Window{HWND h;~Window(){DestroyWindow(h);}} window_owner{hwnd};
    auto* api=Direct3DCreate9(D3D_SDK_VERSION);if(!api)return 77;
    struct Api{IDirect3D9* p;~Api(){p->Release();}} api_owner{api};
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=hwnd;
    pp.BackBufferWidth=16;pp.BackBufferHeight=16;pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D16;
    IDirect3DDevice9* raw=nullptr;
    const auto hr=api->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&raw);
    if(FAILED(hr)){std::fprintf(stderr,"HAL create failed %08lx\n",hr);return 77;}
    struct Device{IDirect3DDevice9* p;~Device(){p->Release();}} device_owner{raw};
    IDirect3DVertexBuffer9* sentinel_vb=nullptr;IDirect3DIndexBuffer9* sentinel_ib=nullptr;
    if(FAILED(raw->CreateVertexBuffer(128,D3DUSAGE_SOFTWAREPROCESSING,D3DFVF_XYZ,D3DPOOL_MANAGED,&sentinel_vb,nullptr)))return 77;
    struct Vertex{IDirect3DVertexBuffer9* p;~Vertex(){p->Release();}} vertex_owner{sentinel_vb};
    if(FAILED(raw->CreateIndexBuffer(6,D3DUSAGE_SOFTWAREPROCESSING,D3DFMT_INDEX16,D3DPOOL_MANAGED,&sentinel_ib,nullptr)))return 77;
    struct Index{IDirect3DIndexBuffer9* p;~Index(){p->Release();}} index_owner{sentinel_ib};
    raw->SetStreamSource(0,sentinel_vb,4,12);raw->SetIndices(sentinel_ib);
    checked_device=raw;checked_vertex=sentinel_vb;checked_index=sentinel_ib;
    D3DDEVICE_CREATION_PARAMETERS actual{};raw->GetCreationParameters(&actual);if(actual.DeviceType!=D3DDEVTYPE_HAL)return 77;
    p::Win32Window window(hwnd);p::D3D9Device device(raw);
    try{
        p::InitAsDLL(module,window,device);struct Scope{~Scope(){p::Deinit();}} scope;
        const auto model=std::filesystem::path(argv[1]).wstring();VerifyOriginalShadowTextureDevice(model.c_str(),device,CheckBindings);
        if(checked_calls!=6)throw std::runtime_error("shadow binding checks missing");
        return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"shadow HAL exception: %s\n",error.what());return 1;}
}
