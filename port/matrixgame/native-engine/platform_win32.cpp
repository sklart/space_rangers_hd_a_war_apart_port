// SPDX-License-Identifier: GPL-2.0-or-later
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <mmsystem.h>
#include "platform_win32.hpp"
#include "DeviceState.hpp"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <cstdio>
#include <map>
#include "buffer_resources.hpp"
namespace srhd_awa::matrixgame::platform3g {
struct D3D9Uploads {
    template<class T> struct Entry {T* value{};std::uint64_t version{};~Entry(){if(value)value->Release();}};
    std::map<std::uint64_t,Entry<IDirect3DVertexBuffer9>> vertices;
    std::map<std::uint64_t,Entry<IDirect3DIndexBuffer9>> indices;
};
#include "device_state_sdk_tables.inc"
namespace {
class WinmmTimerLease final:public TimerPeriodLease {
    std::uint32_t period_;std::size_t& count_;bool active_{};
public:
    WinmmTimerLease(std::uint32_t period,std::size_t& count):period_(period),count_(count) {
        const auto result=timeBeginPeriod(period);
        if(result!=TIMERR_NOERROR)throw DeviceError("timeBeginPeriod",static_cast<DeviceResult>(result));
        active_=true;++count_;
    }
    ~WinmmTimerLease(){Release();}
    DeviceResult Release()noexcept override {
        if(!active_)return DeviceSuccess;
        const auto result=timeEndPeriod(period_);
        if(result==TIMERR_NOERROR){active_=false;--count_;}
        return static_cast<DeviceResult>(result);
    }
};
}
std::unique_ptr<TimerPeriodLease> Win32Window::AcquireTimerPeriod(std::uint32_t period) {
    return std::make_unique<WinmmTimerLease>(period,timer_leases_);
}
namespace {IDirect3DDevice9& DeviceRef(void*);Presentation ReadPresentation(IDirect3DDevice9&);}
LegacyDeviceHandles D3D9Device::ReadLegacyHandles() {
    IDirect3D9* api=nullptr;
    CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).GetDirect3D(&api)),"GetDirect3D");
    struct Owner {IDirect3D9* p;~Owner(){p->Release();}} owner{api};
    // The borrowed device keeps its API alive after this temporary reference.
    return {reinterpret_cast<std::uintptr_t>(api),reinterpret_cast<std::uintptr_t>(device_)};
}
CreationParameters D3D9Device::ReadCreationParameters() {
    D3DDEVICE_CREATION_PARAMETERS p{};
    CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).GetCreationParameters(&p)),"GetCreationParameters");
    return {p.AdapterOrdinal,static_cast<std::uint32_t>(p.DeviceType),p.BehaviorFlags,p.hFocusWindow};
}
DisplayMode D3D9Device::ReadAdapterDisplayMode(std::uint32_t adapter) {
    IDirect3D9* api=nullptr;
    CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).GetDirect3D(&api)),"GetDirect3D");
    struct Owner {IDirect3D9* p;~Owner(){p->Release();}} owner{api};
    D3DDISPLAYMODE mode{};
    CheckDeviceResult(static_cast<DeviceResult>(api->GetAdapterDisplayMode(adapter,&mode)),"GetAdapterDisplayMode");
    return {mode.Width,mode.Height,mode.RefreshRate,static_cast<PixelFormat>(mode.Format)};
}
DeviceResult D3D9Device::CheckMultisample(std::uint32_t adapter,std::uint32_t type,PixelFormat format,bool windowed,std::uint32_t samples) {
    IDirect3D9* api=nullptr;
    CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).GetDirect3D(&api)),"GetDirect3D");
    struct Owner {IDirect3D9* p;~Owner(){p->Release();}} owner{api};
    return static_cast<DeviceResult>(api->CheckDeviceMultiSampleType(adapter,static_cast<D3DDEVTYPE>(type),static_cast<D3DFORMAT>(format),windowed,static_cast<D3DMULTISAMPLE_TYPE>(samples),nullptr));
}
DeviceResult D3D9Device::ResetPresentation(Presentation& value) {
    if(presentation_access_!=PresentationAccess::Exclusive)
        throw Unsupported("borrowed device presentation reset is not owned");
    if(!leases_)throw Unsupported("presentation reset requires captured owner state");
    if(!value.hDeviceWindow || !IsWindow(static_cast<HWND>(value.hDeviceWindow)) ||
       !value.BackBufferWidth || !value.BackBufferHeight)
        throw std::invalid_argument("invalid reset presentation dimensions/window");
    auto& device=DeviceRef(device_);
    ReleaseStagingUploads();
    // Exclusive captures use scalar state instead of reset-blocking state
    // blocks. The owner must release its DEFAULT resources before this call.
    // After a failed Reset only cooperative-state/Reset/IUnknown operations
    // are allowed. Preserve the original retry/fallback path in that state.
    if(device.TestCooperativeLevel()==D3D_OK){
        D3DCAPS9 caps{};CheckDeviceResult(device.GetDeviceCaps(&caps),"reset GetDeviceCaps");
        for(unsigned stage=0;stage<8;++stage)CheckDeviceResult(device.SetTexture(stage,nullptr),"reset unbind texture");
        for(unsigned stream=0;stream<caps.MaxStreams;++stream)
            CheckDeviceResult(device.SetStreamSource(stream,nullptr,0,0),"reset unbind stream");
        CheckDeviceResult(device.SetIndices(nullptr),"reset unbind index");
        CheckDeviceResult(device.SetDepthStencilSurface(nullptr),"reset unbind depth");
        for(unsigned target=1;target<caps.NumSimultaneousRTs;++target)
            CheckDeviceResult(device.SetRenderTarget(target,nullptr),"reset unbind extra target");
    }
    D3DPRESENT_PARAMETERS p{};
    p.BackBufferWidth=value.BackBufferWidth;p.BackBufferHeight=value.BackBufferHeight;
    p.BackBufferFormat=static_cast<D3DFORMAT>(value.BackBufferFormat);p.BackBufferCount=value.BackBufferCount;
    p.MultiSampleType=static_cast<D3DMULTISAMPLE_TYPE>(value.MultiSampleType);p.MultiSampleQuality=value.MultiSampleQuality;
    p.SwapEffect=static_cast<D3DSWAPEFFECT>(value.SwapEffect);p.hDeviceWindow=static_cast<HWND>(value.hDeviceWindow);
    p.Windowed=value.Windowed;p.EnableAutoDepthStencil=value.EnableAutoDepthStencil;
    p.AutoDepthStencilFormat=static_cast<D3DFORMAT>(value.AutoDepthStencilFormat);p.Flags=value.Flags;
    p.FullScreen_RefreshRateInHz=value.FullScreen_RefreshRateInHz;p.PresentationInterval=value.PresentationInterval;
    const auto hr=device.Reset(&p);
    std::fprintf(stderr,"actual D3D9 Reset %ux%u HRESULT=%08lx\n",value.BackBufferWidth,value.BackBufferHeight,static_cast<unsigned long>(hr));
    if(SUCCEEDED(hr))value=ReadPresentation(device);
    return static_cast<DeviceResult>(hr);
}
WindowRect Win32Window::ReadClientRect() {
    RECT r{};if(!GetClientRect(static_cast<HWND>(hwnd_),&r))throw std::runtime_error("GetClientRect failed");
    return {r.left,r.top,r.right,r.bottom};
}
namespace {
D3DCOLORVALUE SdkColor(const LightColor& c){return {c.r,c.g,c.b,c.a};}
D3DVECTOR SdkVector(const LightVector& v){return {v.x,v.y,v.z};}
}
DeviceResult D3D9Device::SetVideoViewport(const VideoViewport& value){
    if(!leases_)throw Unsupported("video viewport requires captured device state");
    D3DVIEWPORT9 viewport{value.X,value.Y,value.Width,value.Height,value.MinZ,value.MaxZ};
    return static_cast<DeviceResult>(DeviceRef(device_).SetViewport(&viewport));
}
DeviceResult D3D9Device::SetVideoMaterial(const VideoMaterial& value){
    if(!leases_)throw Unsupported("video material requires captured device state");
    D3DMATERIAL9 material{SdkColor(value.Diffuse),SdkColor(value.Ambient),SdkColor(value.Specular),SdkColor(value.Emissive),value.Power};
    return static_cast<DeviceResult>(DeviceRef(device_).SetMaterial(&material));
}
DeviceResult D3D9Device::SetVideoLight(std::uint32_t index,const VideoLight& value){
    if(!leases_)throw Unsupported("video light requires captured device state");
    D3DLIGHT9 light{};light.Type=static_cast<D3DLIGHTTYPE>(value.Type);
    light.Diffuse=SdkColor(value.Diffuse);light.Specular=SdkColor(value.Specular);light.Ambient=SdkColor(value.Ambient);
    light.Position=SdkVector(value.Position);light.Direction=SdkVector(value.Direction);
    light.Range=value.Range;light.Falloff=value.Falloff;
    light.Attenuation0=value.Attenuation0;light.Attenuation1=value.Attenuation1;light.Attenuation2=value.Attenuation2;
    light.Theta=value.Theta;light.Phi=value.Phi;
    return static_cast<DeviceResult>(DeviceRef(device_).SetLight(index,&light));
}
DeviceResult D3D9Device::EnableVideoLight(std::uint32_t index,bool enabled){
    if(!leases_)throw Unsupported("video light enable requires captured device state");
    return static_cast<DeviceResult>(DeviceRef(device_).LightEnable(index,enabled));
}
WindowRect Win32Window::ReadWindowRect() {
    RECT r{};if(!GetWindowRect(static_cast<HWND>(hwnd_),&r))throw std::runtime_error("GetWindowRect failed");
    return {r.left,r.top,r.right,r.bottom};
}
void Win32Window::ResizeOuter(std::int32_t width,std::int32_t height) {
    if(width<=0 || height<=0)throw std::invalid_argument("invalid outer window size");
    if(!SetWindowPos(static_cast<HWND>(hwnd_),nullptr,0,0,width,height,
                     SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOMOVE|SWP_NOACTIVATE))
        throw std::runtime_error("SetWindowPos resize failed");
}
void Win32Window::SetFullscreenStyle() {
    SetLastError(0);
    if(!SetWindowLongPtrW(static_cast<HWND>(hwnd_),GWL_STYLE,WS_POPUP|WS_VISIBLE) && GetLastError())
        throw std::runtime_error("SetWindowLongPtr fullscreen failed");
}
void Win32Window::MoveFullscreen(std::int32_t width,std::int32_t height) {
    if(width<=0 || height<=0)throw std::invalid_argument("invalid fullscreen window size");
    if(!MoveWindow(static_cast<HWND>(hwnd_),0,0,width,height,FALSE))throw std::runtime_error("MoveWindow fullscreen failed");
}
namespace {
Presentation FromSdk(const D3DPRESENT_PARAMETERS& p) {
    return {p.BackBufferWidth,p.BackBufferHeight,static_cast<PixelFormat>(p.BackBufferFormat),
        p.BackBufferCount,static_cast<std::uint32_t>(p.MultiSampleType),p.MultiSampleQuality,static_cast<std::uint32_t>(p.SwapEffect),
        p.hDeviceWindow,bool(p.Windowed),bool(p.EnableAutoDepthStencil),static_cast<PixelFormat>(p.AutoDepthStencilFormat),
        p.Flags,p.FullScreen_RefreshRateInHz,p.PresentationInterval};
}
Presentation ReadPresentation(IDirect3DDevice9& device) {
    IDirect3DSwapChain9* chain{};CheckDeviceResult(device.GetSwapChain(0,&chain),"GetSwapChain presentation");
    struct Owner{IDirect3DSwapChain9* p;~Owner(){p->Release();}} owner{chain};
    D3DPRESENT_PARAMETERS parameters{};CheckDeviceResult(chain->GetPresentParameters(&parameters),"GetPresentParameters");
    return FromSdk(parameters);
}
constexpr wchar_t property[]=L"SRHD.MatrixGame.Native3G.Attachment";
struct Attachment final:WindowAttachment {
    HWND hwnd;WNDPROC previous{};Win32Window& window;MessageHandler handler;bool installed{};
    WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};LONG_PTR style{},exstyle{};
    bool visible{};
    Attachment(HWND h,Win32Window& w,MessageHandler f):hwnd(h),window(w),handler(f) {
        if(!IsWindow(hwnd) || GetPropW(hwnd,property))throw std::logic_error("window invalid or already attached");
        if(!GetWindowPlacement(hwnd,&placement))throw std::runtime_error("capture window placement failed");
        style=GetWindowLongPtrW(hwnd,GWL_STYLE);exstyle=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
        visible=IsWindowVisible(hwnd)!=FALSE;
        previous=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd,GWLP_WNDPROC));
        if(!previous)throw std::runtime_error("GetWindowLongPtrW failed");
        if(!SetPropW(hwnd,property,this))throw std::runtime_error("SetPropW failed");
        SetLastError(0);
        const auto replaced=SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&Dispatch));
        if(!replaced && GetLastError()) {RemovePropW(hwnd,property);throw std::runtime_error("SetWindowLongPtrW failed");}
        installed=true;
    }
    ~Attachment(){Restore();}
    static LRESULT CALLBACK Dispatch(HWND h,UINT m,WPARAM w,LPARAM l) {
        auto* self=static_cast<Attachment*>(GetPropW(h,property));
        return self?self->handler(self->window,m,w,l):DefWindowProcW(h,m,w,l);
    }
    void Restore()noexcept override {
        if(!installed)return;
        // Do not overwrite a hook installed later by another owner.
        if(IsWindow(hwnd) && reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd,GWLP_WNDPROC))==&Dispatch)
            SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(previous));
        if(GetPropW(hwnd,property)==this)RemovePropW(hwnd,property);
        if(IsWindow(hwnd)){
            SetWindowLongPtrW(hwnd,GWL_STYLE,style);SetWindowLongPtrW(hwnd,GWL_EXSTYLE,exstyle);
            SetWindowPlacement(hwnd,&placement);
            SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
            if(!visible)ShowWindow(hwnd,SW_HIDE);
        }
        installed=false;
    }
};
IDirect3DDevice9& DeviceRef(void* p) {
    if(!p)throw std::invalid_argument("null borrowed D3D9 device");
    return *static_cast<IDirect3DDevice9*>(p);
}
struct StateLease final:DeviceStateLease {
    IDirect3DStateBlock9* block{};IDirect3DDevice9* device{};std::size_t& live;bool counted{};
    D3DMATRIX world{},view{},projection{};
    D3DVIEWPORT9 viewport{};
    D3DMATERIAL9 material{};
    // The full original TU now owns render/sampler capture and restoration.
    // World, texcoord and COM texture leases were never part of CDeviceState.
    CDeviceState stored;
    static constexpr D3DTEXTURESTAGESTATETYPE stage_keys[]{D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,
        D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_TEXCOORDINDEX,D3DTSS_RESULTARG,D3DTSS_TEXTURETRANSFORMFLAGS};
    DWORD texture_stages[8][std::size(stage_keys)]{};
    D3DMATRIX texture_matrices[8]{};
    DWORD fvf{};IDirect3DVertexDeclaration9* declaration{};
    IDirect3DBaseTexture9* textures[8]{};
    D3D9Device* reset_owner{};Presentation presentation{};
    D3DLIGHT9 light0{};BOOL light0_enabled{};bool has_light0{};
    StateLease(std::size_t& count,StoredStateBackend& backend):live(count),stored(&backend){}
    ~StateLease(){for(auto* texture:textures)if(texture)texture->Release();if(declaration)declaration->Release();if(block)block->Release();if(counted)--live;}
    void CaptureContract() {
        if(reset_owner){
            presentation=ReadPresentation(*device);
            IDirect3DVertexShader9* vs{};IDirect3DPixelShader9* ps{};
            CheckDeviceResult(device->GetVertexShader(&vs),"capture reset vertex shader");
            CheckDeviceResult(device->GetPixelShader(&ps),"capture reset pixel shader");
            const bool programmed=vs || ps;if(vs)vs->Release();if(ps)ps->Release();
            if(programmed)throw Unsupported("exclusive reset snapshot requires an unprogrammed owner baseline");
            IDirect3DSurface9* target{};IDirect3DSurface9* backbuffer{};
            CheckDeviceResult(device->GetRenderTarget(0,&target),"capture reset target");
            const auto hr=device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&backbuffer);
            const bool same=target==backbuffer;if(target)target->Release();if(backbuffer)backbuffer->Release();
            CheckDeviceResult(hr,"capture reset backbuffer");
            if(!same)throw Unsupported("exclusive reset snapshot requires owner backbuffer");
            D3DCAPS9 caps{};CheckDeviceResult(device->GetDeviceCaps(&caps),"capture reset caps");
            for(unsigned stream=0;stream<caps.MaxStreams;++stream){
                IDirect3DVertexBuffer9* buffer{};UINT offset{},stride{};
                CheckDeviceResult(device->GetStreamSource(stream,&buffer,&offset,&stride),"capture reset stream");
                if(buffer){buffer->Release();throw Unsupported("exclusive reset snapshot requires unbound owner streams");}
            }
            IDirect3DIndexBuffer9* index{};CheckDeviceResult(device->GetIndices(&index),"capture reset indices");
            if(index){index->Release();throw Unsupported("exclusive reset snapshot requires unbound owner indices");}
            has_light0=SUCCEEDED(device->GetLight(0,&light0));
            if(has_light0)CheckDeviceResult(device->GetLightEnable(0,&light0_enabled),"capture reset light");
        }
        CheckDeviceResult(static_cast<DeviceResult>(device->GetViewport(&viewport)),"capture viewport");
        CheckDeviceResult(static_cast<DeviceResult>(device->GetMaterial(&material)),"capture material");
        CheckDeviceResult(static_cast<DeviceResult>(device->GetTransform(D3DTS_WORLD,&world)),"capture world");
        CheckDeviceResult(static_cast<DeviceResult>(device->GetTransform(D3DTS_VIEW,&view)),"capture view");
        CheckDeviceResult(static_cast<DeviceResult>(device->GetTransform(D3DTS_PROJECTION,&projection)),"capture projection");
        stored.StoreState(DEVSTATE_RENDERSTATES|DEVSTATE_SAMPLERSTATES);
        CheckDeviceResult(static_cast<DeviceResult>(device->GetFVF(&fvf)),"capture FVF");
        CheckDeviceResult(static_cast<DeviceResult>(device->GetVertexDeclaration(&declaration)),"capture vertex declaration lease");
        for(unsigned stage=0;stage<8;++stage) {
            for(unsigned key=0;key<std::size(stage_keys);++key)
                CheckDeviceResult(static_cast<DeviceResult>(device->GetTextureStageState(stage,stage_keys[key],&texture_stages[stage][key])),"capture texture stage");
            CheckDeviceResult(static_cast<DeviceResult>(device->GetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0+stage),&texture_matrices[stage])),"capture texture matrix");
            CheckDeviceResult(static_cast<DeviceResult>(device->GetTexture(stage,&textures[stage])),"capture texture lease");
            if(reset_owner && textures[stage]){
                // Retaining a DEFAULT-pool texture would prevent Reset.
                D3DSURFACE_DESC desc{};
                if(textures[stage]->GetType()!=D3DRTYPE_TEXTURE)throw Unsupported("exclusive reset snapshot texture type");
                CheckDeviceResult(static_cast<IDirect3DTexture9*>(textures[stage])->GetLevelDesc(0,&desc),"capture reset texture pool");
                if(desc.Pool==D3DPOOL_DEFAULT)throw Unsupported("exclusive reset snapshot retains DEFAULT texture");
            }
        }
    }
    DeviceResult Restore()noexcept override {
        // NULLREF returned success from Apply without restoring observable
        // world/render values. Explicit Get/Set covers every state this native
        // contract can mutate; never treat the block's HRESULT alone as proof.
        DeviceResult result=DeviceSuccess;
        if(reset_owner){
            try {
                reset_owner->ReleaseStagingUploads();
                if(device->TestCooperativeLevel()!=D3D_OK || !(ReadPresentation(*device)==presentation)){
                    auto previous=presentation;result=reset_owner->ResetPresentation(previous);
                    if(result!=DeviceSuccess)return result;
                }
            }catch(const DeviceError& error){return error.result;}
            catch(...){return static_cast<DeviceResult>(D3DERR_INVALIDCALL);}
        }
        if(block)result=static_cast<DeviceResult>(block->Apply());
        const auto record=[&](HRESULT hr){if(result==DeviceSuccess && hr!=D3D_OK)result=static_cast<DeviceResult>(hr);};
        record(device->SetTransform(D3DTS_WORLD,&world));
        record(device->SetTransform(D3DTS_VIEW,&view));
        record(device->SetTransform(D3DTS_PROJECTION,&projection));
        record(device->SetViewport(&viewport));
        record(device->SetMaterial(&material));
        if(reset_owner){
            if(has_light0)record(device->SetLight(0,&light0));
            record(device->LightEnable(0,has_light0?light0_enabled:FALSE));
        }
        record(fvf?device->SetFVF(fvf):device->SetVertexDeclaration(declaration));
        try {stored.RestoreState(DEVSTATE_RENDERSTATES|DEVSTATE_SAMPLERSTATES);}
        catch(const DeviceError& error){if(result==DeviceSuccess)result=error.result;}
        catch(...){if(result==DeviceSuccess)result=static_cast<DeviceResult>(D3DERR_INVALIDCALL);}
        for(unsigned stage=0;stage<8;++stage) {
            for(unsigned key=0;key<std::size(stage_keys);++key)
                record(device->SetTextureStageState(stage,stage_keys[key],texture_stages[stage][key]));
            record(device->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0+stage),&texture_matrices[stage]));
            record(device->SetTexture(stage,textures[stage]));
        }
        return result;
    }
};
void RequireStage(std::uint32_t stage) {
    if(stage>=8)throw Unsupported("native fixed-function state lease covers stages 0..7");
}
template<class T,std::size_t N> void RequireRegister(T value,const T(&supported)[N]) {
    if(std::find(std::begin(supported),std::end(supported),value)==std::end(supported))
        throw Unsupported("device register outside captured native state contract");
}
}
std::unique_ptr<WindowAttachment> Win32Window::Attach(MessageHandler handler) {
    if(!handler)throw std::invalid_argument("null window handler");
    return std::make_unique<Attachment>(static_cast<HWND>(hwnd_),*this,handler);
}
void Win32Window::Show(Visibility v) {
    ShowWindow(static_cast<HWND>(hwnd_),v==Visibility::Maximized?SW_MAXIMIZE:v==Visibility::Restored?SW_RESTORE:v==Visibility::Normal?SW_SHOWNORMAL:SW_MINIMIZE);
}
void Win32Window::ReleaseCursor(){ClipCursor(nullptr);}
void Win32Window::Update(){::UpdateWindow(static_cast<HWND>(hwnd_));}
CursorHandle Win32Window::ExchangeCursor(CursorHandle cursor) {
    return {reinterpret_cast<std::uintptr_t>(::SetCursor(reinterpret_cast<HCURSOR>(cursor.value)))};
}
CursorHandle Win32Window::ReadCursor(){return {reinterpret_cast<std::uintptr_t>(::GetCursor())};}
void Win32Window::DestroyCursorIcon(IconHandle icon) {
    if(!::DestroyIcon(reinterpret_cast<HICON>(icon.value)))throw std::runtime_error("DestroyIcon failed");
}
std::intptr_t Win32Window::DefaultMessage(std::uint32_t m,std::uintptr_t w,std::intptr_t l) {
    return DefWindowProcW(static_cast<HWND>(hwnd_),m,w,l);
}
DeviceCaps D3D9Device::ReadCaps() {
    auto& device=DeviceRef(device_);D3DCAPS9 caps{};
    if(FAILED(device.GetDeviceCaps(&caps)))throw std::runtime_error("IDirect3DDevice9::GetDeviceCaps failed");
    return {bool(caps.Caps2&D3DCAPS2_CANCALIBRATEGAMMA),caps.MaxTextureBlendStages,caps.MaxAnisotropy,device.GetNumberOfSwapChains(),caps.MaxSimultaneousTextures};
}
DeviceResult D3D9Device::SetFixedVertexLayout(std::uint32_t layout) {
    if(!leases_)throw Unsupported("fixed vertex layout requires a captured state lease");
    return static_cast<DeviceResult>(DeviceRef(device_).SetFVF(layout));
}
DeviceResult D3D9Device::BindBorrowedTexture(std::uint32_t stage,void* texture) {
    RequireStage(stage);if(!leases_)throw Unsupported("texture binding requires a captured state lease");
    return static_cast<DeviceResult>(DeviceRef(device_).SetTexture(stage,static_cast<IDirect3DTexture9*>(texture)));
}
DeviceResult D3D9Device::SetTextureMatrix(std::uint32_t stage,const WorldMatrix& value) {
    if(!leases_)throw Unsupported("texture matrix requires a captured state lease");
    if(stage>=8)throw std::out_of_range("texture matrix stage");
    D3DMATRIX matrix;static_assert(sizeof(matrix)==sizeof(value));
    std::memcpy(&matrix,value.data(),sizeof(matrix));
    return static_cast<DeviceResult>(DeviceRef(device_).SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(D3DTS_TEXTURE0+stage),&matrix));
}
GammaRamp D3D9Device::ReadGamma(std::uint32_t index) {
    auto& device=DeviceRef(device_);
    if(index>=device.GetNumberOfSwapChains())throw std::out_of_range("gamma swapchain");
    D3DGAMMARAMP ramp{};device.GetGammaRamp(index,&ramp);GammaRamp result;
    std::copy_n(ramp.red,256,result.red.begin());std::copy_n(ramp.green,256,result.green.begin());std::copy_n(ramp.blue,256,result.blue.begin());
    return result;
}
void D3D9Device::ApplyCalibratedGamma(std::uint32_t index,const GammaRamp& value) {
    auto& state=Current();
    if(!state.ready || state.device!=this || index>=state.gamma_saved.size())
        throw Unsupported("calibrated gamma requires this device's active owner");
    // Original ApplyGammaRamp is unconditional even without CANCALIBRATEGAMMA.
    // Capture before its first write as well as the optional Init-time capture.
    if(!state.gamma_saved[index]){
        state.saved_gamma[index]=ReadGamma(index);
        state.gamma_saved[index]=true;
    }
    D3DGAMMARAMP ramp{};
    std::copy_n(value.red.begin(),256,ramp.red);std::copy_n(value.green.begin(),256,ramp.green);std::copy_n(value.blue.begin(),256,ramp.blue);
    DeviceRef(device_).SetGammaRamp(index,D3DSGR_CALIBRATE,&ramp);
}
void D3D9Device::RestoreGamma(std::uint32_t index,const GammaRamp& saved)noexcept {
    auto& device=*static_cast<IDirect3DDevice9*>(device_);D3DGAMMARAMP ramp{};
    std::copy_n(saved.red.begin(),256,ramp.red);std::copy_n(saved.green.begin(),256,ramp.green);std::copy_n(saved.blue.begin(),256,ramp.blue);
    device.SetGammaRamp(index,0,&ramp);
}
std::uint64_t D3D9Device::AvailableTextureBytes(){return DeviceRef(device_).GetAvailableTextureMem();}
DeviceResult D3D9Device::SetSamplerState(std::uint32_t sampler,SamplerRegister type,std::uint32_t value) {
    RequireStage(sampler);RequireRegister(static_cast<D3DSAMPLERSTATETYPE>(type),stored_sampler_keys);
    return static_cast<DeviceResult>(DeviceRef(device_).SetSamplerState(sampler,static_cast<D3DSAMPLERSTATETYPE>(type),value));
}
std::uint32_t D3D9Device::ReadSamplerState(std::uint32_t sampler,SamplerRegister type) {
    RequireStage(sampler);RequireRegister(static_cast<D3DSAMPLERSTATETYPE>(type),stored_sampler_keys);
    DWORD value{};
    if(FAILED(DeviceRef(device_).GetSamplerState(sampler,static_cast<D3DSAMPLERSTATETYPE>(type),&value)))
        throw std::runtime_error("IDirect3DDevice9::GetSamplerState failed");
    return value;
}
std::unique_ptr<DeviceStateLease> D3D9Device::CaptureState() {
    auto state=std::make_unique<StateLease>(leases_,*this);
    state->device=&DeviceRef(device_);
    if(presentation_access_==PresentationAccess::Exclusive)state->reset_owner=this;
    else {
        CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).CreateStateBlock(D3DSBT_ALL,&state->block)),"CreateStateBlock");
        if(!state->block)throw std::runtime_error("CreateStateBlock returned no state block");
        CheckDeviceResult(static_cast<DeviceResult>(state->block->Capture()),"StateBlock Capture");
    }
    ++leases_;state->counted=true;
    state->CaptureContract();
    return state;
}
DeviceResult D3D9Device::SetWorldTransform(const WorldMatrix& world) {
    D3DMATRIX matrix;static_assert(sizeof(matrix)==sizeof(world));std::memcpy(&matrix,world.data(),sizeof(matrix));
    return static_cast<DeviceResult>(DeviceRef(device_).SetTransform(D3DTS_WORLD,&matrix));
}
WorldMatrix D3D9Device::ReadWorldTransform() {
    D3DMATRIX matrix{};CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).GetTransform(D3DTS_WORLD,&matrix)),"GetTransform");
    WorldMatrix out;std::memcpy(out.data(),&matrix,sizeof(matrix));return out;
}
DeviceResult D3D9Device::SetRenderState(RasterRegister type,std::uint32_t value) {
    RequireRegister(static_cast<D3DRENDERSTATETYPE>(type),stored_render_keys);
    return static_cast<DeviceResult>(DeviceRef(device_).SetRenderState(static_cast<D3DRENDERSTATETYPE>(type),value));
}
std::uint32_t D3D9Device::ReadRenderState(RasterRegister type) {
    RequireRegister(static_cast<D3DRENDERSTATETYPE>(type),stored_render_keys);
    DWORD value{};CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).GetRenderState(static_cast<D3DRENDERSTATETYPE>(type),&value)),"GetRenderState");return value;
}
DeviceResult D3D9Device::ClearTexture(std::uint32_t stage) {RequireStage(stage);return static_cast<DeviceResult>(DeviceRef(device_).SetTexture(stage,nullptr));}
void D3D9Device::ReleaseStagingUploads()noexcept {
    if(!uploads_)return;
    auto& device=*static_cast<IDirect3DDevice9*>(device_);
    if(device.TestCooperativeLevel()==D3D_OK){device.SetStreamSource(0,nullptr,0,0);device.SetIndices(nullptr);}
    uploads_.reset();
}
namespace {
DWORD StagingUsage(IDirect3DDevice9& device){
    D3DDEVICE_CREATION_PARAMETERS parameters{};
    CheckDeviceResult(device.GetCreationParameters(&parameters),"staging GetCreationParameters");
    return D3DUSAGE_WRITEONLY | ((parameters.BehaviorFlags&(D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MIXED_VERTEXPROCESSING))?D3DUSAGE_SOFTWAREPROCESSING:0);
}
template<class T> void Upload(T* target,const buffers::Resource& source){
    void* bytes{};CheckDeviceResult(target->Lock(0,0,&bytes,0),"staging upload Lock");
    std::memcpy(bytes,source.Storage().data(),source.Size());
    CheckDeviceResult(target->Unlock(),"staging upload Unlock");
}
}
DeviceResult D3D9Device::BindStagingVertex(std::uint32_t stream,buffers::VertexBuffer* source,std::uint32_t offset,std::uint32_t stride){
    if(stream!=0)throw Unsupported("staging stream outside native single-stream contract");
    auto& device=DeviceRef(device_);
    if(!source){
        const auto result=device.SetStreamSource(stream,nullptr,0,0);
        if(SUCCEEDED(result))buffers::Current().BindVertex(nullptr,0,0);
        return result;
    }
    if(source->Locked() || !stride || offset>=source->Size())throw std::invalid_argument("invalid staging vertex binding");
    if(!uploads_)uploads_=std::make_shared<D3D9Uploads>();
    auto& entry=uploads_->vertices[source->Id()];
    if(!entry.value)CheckDeviceResult(device.CreateVertexBuffer(static_cast<UINT>(source->Size()),StagingUsage(device),source->Layout(),D3DPOOL_MANAGED,&entry.value,nullptr),"CreateVertexBuffer staging");
    if(entry.version!=source->Version()){Upload(entry.value,*source);entry.version=source->Version();}
    const auto result=device.SetStreamSource(stream,entry.value,offset,stride);
    if(SUCCEEDED(result))buffers::Current().BindVertex(source,offset,stride);
    return result;
}
DeviceResult D3D9Device::BindStagingIndex(buffers::IndexBuffer* source){
    auto& device=DeviceRef(device_);
    if(!source){
        const auto result=device.SetIndices(nullptr);
        if(SUCCEEDED(result))buffers::Current().BindIndex(nullptr);
        return result;
    }
    if(source->Locked())throw std::invalid_argument("locked staging index binding");
    if(!uploads_)uploads_=std::make_shared<D3D9Uploads>();
    auto& entry=uploads_->indices[source->Id()];
    if(!entry.value)CheckDeviceResult(device.CreateIndexBuffer(static_cast<UINT>(source->Size()),StagingUsage(device),source->Width()==buffers::IndexWidth::U16?D3DFMT_INDEX16:D3DFMT_INDEX32,D3DPOOL_MANAGED,&entry.value,nullptr),"CreateIndexBuffer staging");
    if(entry.version!=source->Version()){Upload(entry.value,*source);entry.version=source->Version();}
    const auto result=device.SetIndices(entry.value);
    if(SUCCEEDED(result))buffers::Current().BindIndex(source);
    return result;
}
bool D3D9Device::TextureBound(std::uint32_t stage) {
    RequireStage(stage);
    IDirect3DBaseTexture9* texture{};CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).GetTexture(stage,&texture)),"GetTexture");
    const bool bound=texture!=nullptr;if(texture)texture->Release();return bound;
}
DeviceResult D3D9Device::SetTextureStageState(std::uint32_t stage,TextureStageRegister type,std::uint32_t value) {
    RequireStage(stage);RequireRegister(static_cast<D3DTEXTURESTAGESTATETYPE>(type),StateLease::stage_keys);
    return static_cast<DeviceResult>(DeviceRef(device_).SetTextureStageState(stage,static_cast<D3DTEXTURESTAGESTATETYPE>(type),value));
}
std::uint32_t D3D9Device::ReadTextureStageState(std::uint32_t stage,TextureStageRegister type) {
    RequireStage(stage);RequireRegister(static_cast<D3DTEXTURESTAGESTATETYPE>(type),StateLease::stage_keys);
    DWORD value{};CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).GetTextureStageState(stage,static_cast<D3DTEXTURESTAGESTATETYPE>(type),&value)),"GetTextureStageState");return value;
}
static_assert(static_cast<unsigned>(RasterRegister::ShadeMode)==D3DRS_SHADEMODE && static_cast<unsigned>(RasterRegister::CullMode)==D3DRS_CULLMODE && static_cast<unsigned>(RasterRegister::Lighting)==D3DRS_LIGHTING);
static_assert(static_cast<unsigned>(TextureStageRegister::TexcoordIndex)==D3DTSS_TEXCOORDINDEX);
static_assert(D3DSHADE_GOURAUD==2 && D3DCULL_CCW==3 && D3DZB_TRUE==1);
DeviceResult D3D9Device::GetRenderState(StoredRenderRegister key,std::uint32_t* out) {
    const auto i=static_cast<unsigned>(key);
    if(i>=std::size(stored_render_keys)||!out)throw std::invalid_argument("stored render key/output");
    DWORD value{};const auto hr=DeviceRef(device_).GetRenderState(stored_render_keys[i],&value);
    if(SUCCEEDED(hr))*out=value;return static_cast<DeviceResult>(hr);
}
DeviceResult D3D9Device::SetRenderState(StoredRenderRegister key,std::uint32_t value) {
    const auto i=static_cast<unsigned>(key);
    if(i>=std::size(stored_render_keys))throw std::invalid_argument("stored render key");
    return static_cast<DeviceResult>(DeviceRef(device_).SetRenderState(stored_render_keys[i],value));
}
DeviceResult D3D9Device::GetSamplerState(std::uint32_t stage,StoredSamplerRegister key,std::uint32_t* out) {
    RequireStage(stage);const auto i=static_cast<unsigned>(key);
    if(i>=std::size(stored_sampler_keys)||!out)throw std::invalid_argument("stored sampler key/output");
    DWORD value{};const auto hr=DeviceRef(device_).GetSamplerState(stage,stored_sampler_keys[i],&value);
    if(SUCCEEDED(hr))*out=value;return static_cast<DeviceResult>(hr);
}
DeviceResult D3D9Device::SetSamplerState(std::uint32_t stage,StoredSamplerRegister key,std::uint32_t value) {
    RequireStage(stage);const auto i=static_cast<unsigned>(key);
    if(i>=std::size(stored_sampler_keys))throw std::invalid_argument("stored sampler key");
    return static_cast<DeviceResult>(DeviceRef(device_).SetSamplerState(stage,stored_sampler_keys[i],value));
}
namespace {
std::array<float,4> RawColor(const D3DCOLORVALUE& c){return {c.r,c.g,c.b,c.a};}
D3DCOLORVALUE SdkColor(const std::array<float,4>& c){return {c[0],c[1],c[2],c[3]};}
}
DeviceResult D3D9Device::GetLight(std::uint32_t index,StoredLight* out){
    if(index>=8 || !out)throw std::invalid_argument("stored light index/output");
    D3DLIGHT9 l{};const auto result=DeviceRef(device_).GetLight(index,&l);
    if(SUCCEEDED(result))*out={static_cast<std::uint32_t>(l.Type),RawColor(l.Diffuse),RawColor(l.Specular),RawColor(l.Ambient),
        {l.Position.x,l.Position.y,l.Position.z},{l.Direction.x,l.Direction.y,l.Direction.z},l.Range,l.Falloff,l.Attenuation0,l.Attenuation1,l.Attenuation2,l.Theta,l.Phi};
    return static_cast<DeviceResult>(result);
}
DeviceResult D3D9Device::GetLightEnable(std::uint32_t index,std::int32_t* out){
    if(index>=8 || !out)throw std::invalid_argument("stored light enable index/output");
    BOOL enabled{};const auto result=DeviceRef(device_).GetLightEnable(index,&enabled);
    if(SUCCEEDED(result))*out=enabled;return static_cast<DeviceResult>(result);
}
DeviceResult D3D9Device::SetLight(std::uint32_t index,const StoredLight* value){
    if(index>=8 || !value)throw std::invalid_argument("stored light index/input");
    if(!leases_)throw Unsupported("stored light restore requires captured device state");
    const auto& v=*value;D3DLIGHT9 l{};l.Type=static_cast<D3DLIGHTTYPE>(v.Type);
    l.Diffuse=SdkColor(v.Diffuse);l.Specular=SdkColor(v.Specular);l.Ambient=SdkColor(v.Ambient);
    l.Position={v.Position[0],v.Position[1],v.Position[2]};l.Direction={v.Direction[0],v.Direction[1],v.Direction[2]};
    l.Range=v.Range;l.Falloff=v.Falloff;l.Attenuation0=v.Attenuation0;l.Attenuation1=v.Attenuation1;l.Attenuation2=v.Attenuation2;l.Theta=v.Theta;l.Phi=v.Phi;
    return static_cast<DeviceResult>(DeviceRef(device_).SetLight(index,&l));
}
DeviceResult D3D9Device::LightEnable(std::uint32_t index,std::int32_t enabled){
    if(index>=8)throw std::invalid_argument("stored light enable index");
    if(!leases_)throw Unsupported("stored light enable restore requires captured device state");
    return static_cast<DeviceResult>(DeviceRef(device_).LightEnable(index,enabled));
}
DeviceResult D3D9Device::GetMaterial(StoredMaterial* out){
    if(!out)throw std::invalid_argument("stored material output");
    D3DMATERIAL9 m{};const auto result=DeviceRef(device_).GetMaterial(&m);
    if(SUCCEEDED(result))*out={RawColor(m.Diffuse),RawColor(m.Ambient),RawColor(m.Specular),RawColor(m.Emissive),m.Power};
    return static_cast<DeviceResult>(result);
}
DeviceResult D3D9Device::SetMaterial(const StoredMaterial* value){
    if(!value)throw std::invalid_argument("stored material input");
    if(!leases_)throw Unsupported("stored material restore requires captured device state");
    const auto& v=*value;D3DMATERIAL9 m{SdkColor(v.Diffuse),SdkColor(v.Ambient),SdkColor(v.Specular),SdkColor(v.Emissive),v.Power};
    return static_cast<DeviceResult>(DeviceRef(device_).SetMaterial(&m));
}
void D3D9Device::CaptureLightSlot(std::uint32_t index,StoredLight* light,std::int32_t* enabled){
    if(index>=8 || !light || !enabled)throw std::invalid_argument("stored light slot index/output");
    StoredLight value{};std::int32_t on{};
    const auto definition=GetLight(index,&value),activation=GetLightEnable(index,&on);
    if(definition==static_cast<DeviceResult>(D3DERR_INVALIDCALL) && activation==static_cast<DeviceResult>(D3DERR_INVALIDCALL)){
        // A never-assigned D3D9 slot is absent, not a failed capture. The
        // original zeroed Type and skipped SetLight for this slot on restore.
        *light={};*enabled=0;return;
    }
    CheckDeviceResult(definition,"capture SDK light definition");
    CheckDeviceResult(activation,"capture SDK light activation");
    *light=value;*enabled=on;
}
}
