// SPDX-License-Identifier: GPL-2.0-or-later
// Actual D3D9 HAL Reset ownership oracle. No game/GPU emulation.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d9.h>
#include <cassert>
#include <cstdio>
#include <stdexcept>
void Check(HRESULT r){if(FAILED(r))throw std::runtime_error("HAL setup/reset failed");}
int main(){
    std::setvbuf(stdout,nullptr,_IONBF,0);
    HWND window=CreateWindowExW(0,L"STATIC",L"MatrixGame Reset ownership oracle",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    assert(window);auto* api=Direct3DCreate9(D3D_SDK_VERSION);assert(api);
    D3DPRESENT_PARAMETERS initial{};initial.Windowed=TRUE;initial.hDeviceWindow=window;initial.SwapEffect=D3DSWAPEFFECT_DISCARD;initial.BackBufferWidth=128;initial.BackBufferHeight=128;initial.BackBufferFormat=D3DFMT_X8R8G8B8;
    IDirect3DDevice9* device{};Check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&initial,&device));
    unsigned cases{},failures{};
    D3DADAPTER_IDENTIFIER9 adapter{};Check(api->GetAdapterIdentifier(0,0,&adapter));
    std::printf("HAL_ADAPTER %s driver=%s version=%08lx:%08lx vendor=%04lx device=%04lx\n",adapter.Description,adapter.Driver,adapter.DriverVersion.HighPart,adapter.DriverVersion.LowPart,adapter.VendorId,adapter.DeviceId);
    const auto fresh=[&]{device->Release();device=nullptr;auto parameters=initial;Check(api->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&parameters,&device));};
    const auto reset=[&](const char* name,bool pass){auto parameters=initial;const auto result=device->Reset(&parameters);std::printf("HAL_RESET %s hr=%08lx expected_success=%u\n",name,static_cast<unsigned long>(result),unsigned(pass));failures+=SUCCEEDED(result)!=pass;++cases;};
    const auto texture=[&](D3DPOOL pool){IDirect3DTexture9* value{};Check(device->CreateTexture(16,16,1,0,D3DFMT_A8R8G8B8,pool,&value,nullptr));return value;};
    {
        auto* value=texture(D3DPOOL_DEFAULT);Check(device->SetTexture(0,value));value->Release();
        reset("default_texture_device_only",true);
    }
    {
        fresh();auto* value=texture(D3DPOOL_DEFAULT);Check(device->SetTexture(0,value));Check(device->SetTexture(3,value));value->Release();
        reset("default_texture_two_bindings_only",true);
    }
    {
        fresh();auto* value=texture(D3DPOOL_DEFAULT);Check(device->SetTexture(0,value));
        reset("default_texture_external_owner",false);value->Release();reset("external_owner_released",true);
    }
    {
        fresh();auto* value=texture(D3DPOOL_DEFAULT);Check(device->SetTexture(1,value));IDirect3DStateBlock9* state{};Check(device->CreateStateBlock(D3DSBT_ALL,&state));Check(state->Capture());value->Release();
        reset("state_block_holds_default_texture",false);const auto refs=state->Release();std::printf("HAL state block Release refs=%lu cooperative=%08lx\n",refs,static_cast<unsigned long>(device->TestCooperativeLevel()));reset("state_block_released",true);
    }
    {
        fresh();auto* value=texture(D3DPOOL_DEFAULT);Check(device->SetTexture(1,value));IDirect3DStateBlock9* state{};Check(device->CreateStateBlock(D3DSBT_ALL,&state));value->Release();state->Release();
        reset("state_block_released_before_first_reset",true);
    }
    {
        fresh();auto* value=texture(D3DPOOL_DEFAULT);Check(device->SetTexture(1,value));IDirect3DStateBlock9* state{};Check(device->CreateStateBlock(D3DSBT_ALL,&state));Check(device->SetTexture(1,nullptr));value->Release();
        reset("unbound_state_block_sole_owner",false);state->Release();reset("unbound_state_block_released_retry",true);
    }
    {
        fresh();IDirect3DStateBlock9* state{};Check(device->CreateStateBlock(D3DSBT_ALL,&state));state->Release();reset("empty_state_block_released_before_reset",true);
    }
    {
        fresh();IDirect3DSurface9* value{};Check(device->CreateRenderTarget(16,16,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&value,nullptr));Check(device->SetRenderTarget(0,value));value->Release();
        reset("standalone_target_device_only",true);
    }
    {
        fresh();IDirect3DSurface9* value{};Check(device->CreateDepthStencilSurface(128,128,D3DFMT_D24S8,D3DMULTISAMPLE_NONE,0,TRUE,&value,nullptr));Check(device->SetDepthStencilSurface(value));value->Release();
        reset("depth_device_only",true);
    }
    {
        fresh();auto* value=texture(D3DPOOL_DEFAULT);IDirect3DSurface9* surface{};Check(value->GetSurfaceLevel(0,&surface));Check(device->SetTexture(0,value));value->Release();
        reset("surface_level_external_owner",false);surface->Release();reset("surface_level_released",true);
    }
    {
        fresh();auto* value=texture(D3DPOOL_MANAGED);Check(device->SetTexture(0,value));reset("managed_external_owner",true);value->Release();
    }
    device->Release();api->Release();DestroyWindow(window);
    std::printf("actual HAL Reset ownership cases=%u mismatches=%u NO_SWITCH_HARDWARE\n",cases,failures);return failures?10:0;
}
