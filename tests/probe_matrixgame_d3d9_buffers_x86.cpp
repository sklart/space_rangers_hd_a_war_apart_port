// Actual Windows D3D9 resource API reference. No draw or battle is executed.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
int main() {
    HWND window=CreateWindowExW(0,L"STATIC",L"MatrixGame buffer API reference",WS_POPUP,
        0,0,16,16,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window) return 2;
    IDirect3D9* api=Direct3DCreate9(D3D_SDK_VERSION);if(!api) return 3;
    D3DPRESENT_PARAMETERS params{};params.Windowed=TRUE;params.SwapEffect=D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow=window;params.BackBufferWidth=16;params.BackBufferHeight=16;
    params.BackBufferFormat=D3DFMT_UNKNOWN;params.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    IDirect3DDevice9* device=nullptr;D3DDEVTYPE selected=D3DDEVTYPE_NULLREF;
    HRESULT result=api->CreateDevice(0,selected,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&device);
    if(FAILED(result)) { selected=D3DDEVTYPE_HAL;result=api->CreateDevice(0,selected,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&device); }
    if(FAILED(result)) { std::printf("UNAVAILABLE CreateDevice=%08lx\n",static_cast<unsigned long>(result));api->Release();DestroyWindow(window);return 77; }
    IDirect3DVertexBuffer9* buffer=nullptr;
    result=device->CreateVertexBuffer(96,D3DUSAGE_WRITEONLY,0x112,D3DPOOL_DEFAULT,&buffer,nullptr);
    if(FAILED(result)) return 4;
    void* mapped=nullptr;result=buffer->Lock(0,0,&mapped,0);if(FAILED(result)) return 5;
    for(unsigned i=0;i<96;++i) static_cast<std::uint8_t*>(mapped)[i]=std::uint8_t(i);
    if(FAILED(buffer->Unlock())) return 6;
    const ULONG refs_before=buffer->AddRef();buffer->Release();
    if(FAILED(device->SetStreamSource(0,buffer,0,32))) return 7;
    const ULONG refs_bound=buffer->AddRef();buffer->Release();
    const ULONG refs_released=buffer->Release();buffer=nullptr;
    UINT offset=0,stride=0;
    result=device->GetStreamSource(0,&buffer,&offset,&stride);
    bool bytes_equal=false;
    if(SUCCEEDED(result)&&buffer) {
        if(SUCCEEDED(buffer->Lock(0,0,&mapped,0))) {
            bytes_equal=true;for(unsigned i=0;i<96;++i) bytes_equal&=static_cast<std::uint8_t*>(mapped)[i]==std::uint8_t(i);
            buffer->Unlock();
        }
        buffer->Release();buffer=nullptr;
    }
    device->SetStreamSource(0,nullptr,0,0);
    std::printf("device_type=%u refs_probe_before=%lu refs_probe_bound=%lu refs_after_owner_release=%lu get_hr=%08lx offset=%u stride=%u bytes_equal=%d\n",
        unsigned(selected),refs_before,refs_bound,refs_released,static_cast<unsigned long>(result),offset,stride,int(bytes_equal));
    device->Release();api->Release();DestroyWindow(window);
    return bytes_equal&&offset==0&&stride==32 ? 0 : 8;
}
