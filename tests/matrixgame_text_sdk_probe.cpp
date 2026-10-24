#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <d3dx9core.h>
#include "platform_3g.hpp"
#include <cassert>
#include <cstdio>
int ProbeOriginalTextFontCache(ID3DXFont*(*get)(IDirect3DDevice9*),void(*clear)()) {
    namespace p=srhd_awa::matrixgame::platform3g;
    auto* first=reinterpret_cast<IDirect3DDevice9*>(p::ReadyDevice().ReadLegacyHandles().device);assert(first);
    IDirect3D9* api{};assert(SUCCEEDED(first->GetDirect3D(&api)));
    struct ApiOwner{IDirect3D9* value;~ApiOwner(){value->Release();}} api_owner{api};
    D3DDEVICE_CREATION_PARAMETERS creation{};assert(SUCCEEDED(first->GetCreationParameters(&creation)));
    D3DPRESENT_PARAMETERS params{};params.Windowed=TRUE;params.SwapEffect=D3DSWAPEFFECT_DISCARD;params.hDeviceWindow=creation.hFocusWindow;params.BackBufferWidth=16;params.BackBufferHeight=16;
    IDirect3DDevice9* second{};assert(SUCCEEDED(api->CreateDevice(creation.AdapterOrdinal,D3DDEVTYPE_HAL,creation.hFocusWindow,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&second)));
    auto* font=get(second);assert(font);
    IDirect3DDevice9* actual{};assert(SUCCEEDED(font->GetDevice(&actual)));
    const bool correct=actual==second;
    std::printf("original font cache expected=%p actual=%p first=%p matches_new_device=%d\n",second,actual,first,correct);
    actual->Release();if(clear)clear();
    const auto references=second->Release();std::printf("second device Release remaining=%lu\n",references);
    if(clear)assert(references==0);
    return correct?0:1;
}
