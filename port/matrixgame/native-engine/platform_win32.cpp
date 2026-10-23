// SPDX-License-Identifier: GPL-2.0-or-later
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include "platform_win32.hpp"
#include "DeviceState.hpp"
#include <algorithm>
#include <cstring>
#include <iterator>
namespace srhd_awa::matrixgame::platform3g {
namespace {
constexpr wchar_t property[]=L"SRHD.MatrixGame.Native3G.Attachment";
struct Attachment final:WindowAttachment {
    HWND hwnd;WNDPROC previous{};Win32Window& window;MessageHandler handler;bool installed{};
    Attachment(HWND h,Win32Window& w,MessageHandler f):hwnd(h),window(w),handler(f) {
        if(!IsWindow(hwnd) || GetPropW(hwnd,property))throw std::logic_error("window invalid or already attached");
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
        installed=false;
    }
};
IDirect3DDevice9& DeviceRef(void* p) {
    if(!p)throw std::invalid_argument("null borrowed D3D9 device");
    return *static_cast<IDirect3DDevice9*>(p);
}
struct StateLease final:DeviceStateLease {
    IDirect3DStateBlock9* block{};IDirect3DDevice9* device{};std::size_t& live;bool counted{};
    D3DMATRIX world{};
    static constexpr D3DRENDERSTATETYPE render_keys[]{D3DRS_ZENABLE,D3DRS_SHADEMODE,D3DRS_ZWRITEENABLE,D3DRS_ALPHATESTENABLE,
        D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_CULLMODE,D3DRS_ZFUNC,D3DRS_ALPHAREF,D3DRS_ALPHAFUNC,D3DRS_ALPHABLENDENABLE,
        D3DRS_LIGHTING,D3DRS_COLORWRITEENABLE,D3DRS_BLENDOP};
    static constexpr D3DSAMPLERSTATETYPE sampler_keys[]{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_ADDRESSW,D3DSAMP_MAGFILTER,
        D3DSAMP_MINFILTER,D3DSAMP_MIPFILTER,D3DSAMP_MIPMAPLODBIAS,D3DSAMP_MAXANISOTROPY};
    // The full original TU now owns render/sampler capture and restoration.
    // World, texcoord and COM texture leases were never part of CDeviceState.
    CDeviceState stored;
    DWORD texcoords[8]{};
    IDirect3DBaseTexture9* textures[8]{};
    StateLease(std::size_t& count,StoredStateBackend& backend):live(count),stored(&backend){}
    ~StateLease(){for(auto* texture:textures)if(texture)texture->Release();if(block)block->Release();if(counted)--live;}
    void CaptureContract() {
        CheckDeviceResult(static_cast<DeviceResult>(device->GetTransform(D3DTS_WORLD,&world)),"capture world");
        stored.StoreState(DEVSTATE_RENDERSTATES|DEVSTATE_SAMPLERSTATES);
        for(unsigned stage=0;stage<8;++stage) {
            CheckDeviceResult(static_cast<DeviceResult>(device->GetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,&texcoords[stage])),"capture texcoord");
            CheckDeviceResult(static_cast<DeviceResult>(device->GetTexture(stage,&textures[stage])),"capture texture lease");
        }
    }
    DeviceResult Restore()noexcept override {
        // NULLREF returned success from Apply without restoring observable
        // world/render values. Explicit Get/Set covers every state this native
        // contract can mutate; never treat the block's HRESULT alone as proof.
        DeviceResult result=static_cast<DeviceResult>(block->Apply());
        const auto record=[&](HRESULT hr){if(result==DeviceSuccess && hr!=D3D_OK)result=static_cast<DeviceResult>(hr);};
        record(device->SetTransform(D3DTS_WORLD,&world));
        try {stored.RestoreState(DEVSTATE_RENDERSTATES|DEVSTATE_SAMPLERSTATES);}
        catch(const DeviceError& error){if(result==DeviceSuccess)result=error.result;}
        catch(...){if(result==DeviceSuccess)result=static_cast<DeviceResult>(D3DERR_INVALIDCALL);}
        for(unsigned stage=0;stage<8;++stage) {
            record(device->SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,texcoords[stage]));
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
    ShowWindow(static_cast<HWND>(hwnd_),v==Visibility::Maximized?SW_MAXIMIZE:v==Visibility::Restored?SW_RESTORE:SW_MINIMIZE);
}
void Win32Window::ReleaseCursor(){ClipCursor(nullptr);}
std::intptr_t Win32Window::DefaultMessage(std::uint32_t m,std::uintptr_t w,std::intptr_t l) {
    return DefWindowProcW(static_cast<HWND>(hwnd_),m,w,l);
}
DeviceCaps D3D9Device::ReadCaps() {
    auto& device=DeviceRef(device_);D3DCAPS9 caps{};
    if(FAILED(device.GetDeviceCaps(&caps)))throw std::runtime_error("IDirect3DDevice9::GetDeviceCaps failed");
    return {bool(caps.Caps2&D3DCAPS2_CANCALIBRATEGAMMA),caps.MaxTextureBlendStages,caps.MaxAnisotropy,device.GetNumberOfSwapChains()};
}
GammaRamp D3D9Device::ReadGamma(std::uint32_t index) {
    auto& device=DeviceRef(device_);
    if(index>=device.GetNumberOfSwapChains())throw std::out_of_range("gamma swapchain");
    D3DGAMMARAMP ramp{};device.GetGammaRamp(index,&ramp);GammaRamp result;
    std::copy_n(ramp.red,256,result.red.begin());std::copy_n(ramp.green,256,result.green.begin());std::copy_n(ramp.blue,256,result.blue.begin());
    return result;
}
void D3D9Device::RestoreGamma(std::uint32_t index,const GammaRamp& saved)noexcept {
    auto& device=*static_cast<IDirect3DDevice9*>(device_);D3DGAMMARAMP ramp{};
    std::copy_n(saved.red.begin(),256,ramp.red);std::copy_n(saved.green.begin(),256,ramp.green);std::copy_n(saved.blue.begin(),256,ramp.blue);
    device.SetGammaRamp(index,0,&ramp);
}
std::uint64_t D3D9Device::AvailableTextureBytes(){return DeviceRef(device_).GetAvailableTextureMem();}
DeviceResult D3D9Device::SetSamplerState(std::uint32_t sampler,SamplerRegister type,std::uint32_t value) {
    RequireStage(sampler);RequireRegister(static_cast<D3DSAMPLERSTATETYPE>(type),StateLease::sampler_keys);
    return static_cast<DeviceResult>(DeviceRef(device_).SetSamplerState(sampler,static_cast<D3DSAMPLERSTATETYPE>(type),value));
}
std::uint32_t D3D9Device::ReadSamplerState(std::uint32_t sampler,SamplerRegister type) {
    RequireStage(sampler);RequireRegister(static_cast<D3DSAMPLERSTATETYPE>(type),StateLease::sampler_keys);
    DWORD value{};
    if(FAILED(DeviceRef(device_).GetSamplerState(sampler,static_cast<D3DSAMPLERSTATETYPE>(type),&value)))
        throw std::runtime_error("IDirect3DDevice9::GetSamplerState failed");
    return value;
}
std::unique_ptr<DeviceStateLease> D3D9Device::CaptureState() {
    auto state=std::make_unique<StateLease>(leases_,*this);
    state->device=&DeviceRef(device_);
    CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).CreateStateBlock(D3DSBT_ALL,&state->block)),"CreateStateBlock");
    if(!state->block)throw std::runtime_error("CreateStateBlock returned no state block");
    ++leases_;state->counted=true;
    CheckDeviceResult(static_cast<DeviceResult>(state->block->Capture()),"StateBlock Capture");
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
    RequireRegister(static_cast<D3DRENDERSTATETYPE>(type),StateLease::render_keys);
    return static_cast<DeviceResult>(DeviceRef(device_).SetRenderState(static_cast<D3DRENDERSTATETYPE>(type),value));
}
std::uint32_t D3D9Device::ReadRenderState(RasterRegister type) {
    RequireRegister(static_cast<D3DRENDERSTATETYPE>(type),StateLease::render_keys);
    DWORD value{};CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).GetRenderState(static_cast<D3DRENDERSTATETYPE>(type),&value)),"GetRenderState");return value;
}
DeviceResult D3D9Device::ClearTexture(std::uint32_t stage) {RequireStage(stage);return static_cast<DeviceResult>(DeviceRef(device_).SetTexture(stage,nullptr));}
bool D3D9Device::TextureBound(std::uint32_t stage) {
    RequireStage(stage);
    IDirect3DBaseTexture9* texture{};CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).GetTexture(stage,&texture)),"GetTexture");
    const bool bound=texture!=nullptr;if(texture)texture->Release();return bound;
}
DeviceResult D3D9Device::SetTextureStageState(std::uint32_t stage,TextureStageRegister type,std::uint32_t value) {
    RequireStage(stage);if(type!=TextureStageRegister::TexcoordIndex)throw Unsupported("uncaptured texture-stage register");
    return static_cast<DeviceResult>(DeviceRef(device_).SetTextureStageState(stage,static_cast<D3DTEXTURESTAGESTATETYPE>(type),value));
}
std::uint32_t D3D9Device::ReadTextureStageState(std::uint32_t stage,TextureStageRegister type) {
    RequireStage(stage);if(type!=TextureStageRegister::TexcoordIndex)throw Unsupported("uncaptured texture-stage register");
    DWORD value{};CheckDeviceResult(static_cast<DeviceResult>(DeviceRef(device_).GetTextureStageState(stage,static_cast<D3DTEXTURESTAGESTATETYPE>(type),&value)),"GetTextureStageState");return value;
}
static_assert(static_cast<unsigned>(RasterRegister::ShadeMode)==D3DRS_SHADEMODE && static_cast<unsigned>(RasterRegister::CullMode)==D3DRS_CULLMODE && static_cast<unsigned>(RasterRegister::Lighting)==D3DRS_LIGHTING);
static_assert(static_cast<unsigned>(TextureStageRegister::TexcoordIndex)==D3DTSS_TEXCOORDINDEX);
static_assert(D3DSHADE_GOURAUD==2 && D3DCULL_CCW==3 && D3DZB_TRUE==1);
#include "device_state_sdk_tables.inc"
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
}
