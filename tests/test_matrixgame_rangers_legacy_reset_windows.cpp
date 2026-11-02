// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#error Assertions are required
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "rangers_legacy_presentation.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
std::int32_t CallOriginalRangersReset(void*,void*);
namespace {
using Old=srhd_awa::matrixgame::RangersLegacyPresentation;
template<class T>struct Com {T* p{};~Com(){if(p)p->Release();}};
struct Window {HWND h;~Window(){if(h)DestroyWindow(h);}};
// One-byte offset deliberately tests safe access to packed original storage.
struct Guarded {
 std::array<unsigned char,58> bytes{};
 explicit Guarded(const Old& old){bytes.front()=0xa5;bytes.back()=0x5a;std::memcpy(data(),&old,sizeof(old));}
 void* data(){return bytes.data()+1;}
 Old Read(){Old old{};std::memcpy(&old,data(),sizeof(old));return old;}
 void Check(){assert(bytes.front()==0xa5 && bytes.back()==0x5a);}
};
void CheckActual(IDirect3DDevice9* device,HWND window,const Old& legacy,unsigned width,unsigned height){
 Com<IDirect3DSwapChain9> chain;assert(SUCCEEDED(device->GetSwapChain(0,&chain.p)));
 D3DPRESENT_PARAMETERS actual{};assert(SUCCEEDED(chain.p->GetPresentParameters(&actual)));
 assert(actual.BackBufferWidth==width && actual.BackBufferHeight==height && actual.hDeviceWindow==window);
 assert(actual.BackBufferFormat==legacy.BackBufferFormat && actual.BackBufferCount==legacy.BackBufferCount);
 assert(actual.MultiSampleType==legacy.MultiSampleType && actual.MultiSampleQuality==legacy.MultiSampleQuality);
 assert(actual.SwapEffect==legacy.SwapEffect && bool(actual.Windowed)==bool(legacy.Windowed));
 assert(actual.EnableAutoDepthStencil==legacy.EnableAutoDepthStencil && actual.AutoDepthStencilFormat==legacy.AutoDepthStencilFormat);
 assert(actual.Flags==legacy.Flags && actual.FullScreen_RefreshRateInHz==legacy.FullScreenRefreshRateInHz && actual.PresentationInterval==legacy.PresentationInterval);
}
}
int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);
 static_assert(sizeof(void*)==8 && sizeof(D3DPRESENT_PARAMETERS)==64);
 static_assert(offsetof(D3DPRESENT_PARAMETERS,hDeviceWindow)==32 && offsetof(D3DPRESENT_PARAMETERS,Windowed)==40);
 Window window{CreateWindowExW(0,L"STATIC",L"Rangers original Reset reference",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr)};assert(window.h);
 const auto identity=reinterpret_cast<std::uintptr_t>(window.h);assert(identity<=std::numeric_limits<std::uint32_t>::max());
 Com<IDirect3D9> api{Direct3DCreate9(D3D_SDK_VERSION)};assert(api.p);
 D3DPRESENT_PARAMETERS initial{};initial.Windowed=TRUE;initial.SwapEffect=D3DSWAPEFFECT_DISCARD;initial.hDeviceWindow=window.h;initial.BackBufferWidth=640;initial.BackBufferHeight=480;
 Com<IDirect3DDevice9> device;assert(SUCCEEDED(api.p->CreateDevice(0,D3DDEVTYPE_HAL,window.h,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&initial,&device.p)));
 Old requested{};requested.BackBufferWidth=800;requested.BackBufferHeight=600;requested.BackBufferCount=1;
 requested.SwapEffect=D3DSWAPEFFECT_DISCARD;requested.DeviceWindow=static_cast<std::uint32_t>(identity);
 requested.Windowed=TRUE;requested.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
 Guarded first(requested);assert(SUCCEEDED(CallOriginalRangersReset(device.p,first.data())));first.Check();
 CheckActual(device.p,window.h,first.Read(),800,600);
 std::puts("RANGERS_RESET original_inline_entry=PASS real_HAL_Reset=PASS SDK_in_out_copy=PASS unaligned_56byte_canaries=PASS");
 requested.BackBufferWidth=640;requested.BackBufferHeight=480;
 Com<IDirect3DSurface9> retained;assert(SUCCEEDED(device.p->CreateRenderTarget(32,32,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&retained.p,nullptr)));
 Guarded retry(requested);const auto failed=CallOriginalRangersReset(device.p,retry.data());retry.Check();assert(failed==D3DERR_INVALIDCALL);
 retained.p->Release();retained.p=nullptr;
 assert(SUCCEEDED(CallOriginalRangersReset(device.p,retry.data())));retry.Check();CheckActual(device.p,window.h,retry.Read(),640,480);
 assert(CallOriginalRangersReset(nullptr,retry.data())==D3DERR_INVALIDCALL && CallOriginalRangersReset(device.p,nullptr)==D3DERR_INVALIDCALL);retry.Check();
 std::printf("RANGERS_RESET retained_DEFAULT_failure=%08x actual_release_retry=PASS null_inputs=REJECTED cleanup=RAII no_game_save=PASS\n",unsigned(failed));
 {
  srhd_awa::matrixgame::RangersLegacyWindowBinding binding(0xdeadbeefu,window.h);
  bool duplicate=false;try{srhd_awa::matrixgame::RangersLegacyWindowBinding other(1,window.h);}catch(const std::logic_error&){duplicate=true;}assert(duplicate);
  requested.DeviceWindow=0xdeadbeefu;Guarded token(requested);
  assert(SUCCEEDED(CallOriginalRangersReset(device.p,token.data())));token.Check();assert(token.Read().DeviceWindow==0xdeadbeefu);
  CheckActual(device.p,window.h,token.Read(),640,480);
  requested.DeviceWindow=0xffffffffu;Guarded wrong(requested);const auto before=wrong.bytes;
  assert(CallOriginalRangersReset(device.p,wrong.data())==D3DERR_INVALIDCALL && wrong.bytes==before);
 }
 requested.DeviceWindow=static_cast<std::uint32_t>(identity);Guarded physical(requested);
 assert(SUCCEEDED(CallOriginalRangersReset(device.p,physical.data())));physical.Check();
 std::puts("RANGERS_RESET explicit_token_full_window=PASS token_copyback=PASS wrong_token_unchanged=PASS duplicate_binding=REJECTED scope_release=PASS");
}
