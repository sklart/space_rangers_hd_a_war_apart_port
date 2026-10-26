// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "device_state_backend.hpp"
namespace srhd_awa::matrixgame::platform3g {
struct D3D9Uploads;
// Real Windows SDK implementation, borrowed HWND / IDirect3DDevice9.
// Opaque pointers keep SDK types out of the portable engine contract.
class Win32Window final:public Window {
public:
    explicit Win32Window(void* hwnd):hwnd_(hwnd){}
    void* Identity()const noexcept override{return hwnd_;}
    std::unique_ptr<WindowAttachment> Attach(MessageHandler) override;
    void Show(Visibility) override;
    void ReleaseCursor() override;
    std::intptr_t DefaultMessage(std::uint32_t,std::uintptr_t,std::intptr_t) override;
    CursorHandle ExchangeCursor(CursorHandle)override;
    CursorHandle ReadCursor()override;
    void DestroyCursorIcon(IconHandle)override;
    void Update()override;
    WindowRect ReadClientRect()override;
    WindowRect ReadWindowRect()override;
    void ResizeOuter(std::int32_t,std::int32_t)override;
    void SetFullscreenStyle()override;
    void MoveFullscreen(std::int32_t,std::int32_t)override;
    std::unique_ptr<TimerPeriodLease> AcquireTimerPeriod(std::uint32_t)override;
    std::size_t OutstandingTimerLeases()const noexcept{return timer_leases_;}
private:void* hwnd_;std::size_t timer_leases_{};
};
class D3D9Device final:public Device,public StoredStateBackend {
public:
    enum class PresentationAccess {Borrowed,Exclusive};
    // Exclusive is an owner assertion: no foreign default-pool resources or
    // state blocks may survive a reset. The borrowed mode never resets.
    explicit D3D9Device(void* device,PresentationAccess access=PresentationAccess::Borrowed):device_(device),presentation_access_(access){}
    D3D9Device(const D3D9Device&)=delete;
    D3D9Device& operator=(const D3D9Device&)=delete;
    DeviceCaps ReadCaps() override;
    LegacyDeviceHandles ReadLegacyHandles()override;
    CreationParameters ReadCreationParameters()override;
    DisplayMode ReadAdapterDisplayMode(std::uint32_t)override;
    DeviceResult CheckMultisample(std::uint32_t,std::uint32_t,PixelFormat,bool,std::uint32_t)override;
    DeviceResult ResetPresentation(Presentation&)override;
    DeviceResult SetVideoViewport(const VideoViewport&)override;
    DeviceResult SetVideoMaterial(const VideoMaterial&)override;
    DeviceResult SetVideoLight(std::uint32_t,const VideoLight&)override;
    DeviceResult EnableVideoLight(std::uint32_t,bool)override;
    GammaRamp ReadGamma(std::uint32_t) override;
    void ApplyCalibratedGamma(std::uint32_t,const GammaRamp&)override;
    void RestoreGamma(std::uint32_t,const GammaRamp&)noexcept override;
    std::uint64_t AvailableTextureBytes() override;
    DeviceResult SetSamplerState(std::uint32_t,SamplerRegister,std::uint32_t) override;
    std::uint32_t ReadSamplerState(std::uint32_t,SamplerRegister) override;
    std::unique_ptr<DeviceStateLease> CaptureState()override;
    std::size_t OutstandingStateLeases()const override{return leases_;}
    DeviceResult SetWorldTransform(const WorldMatrix&)override;
    WorldMatrix ReadWorldTransform()override;
    DeviceResult SetRenderState(RasterRegister,std::uint32_t)override;
    std::uint32_t ReadRenderState(RasterRegister)override;
    DeviceResult ClearTexture(std::uint32_t)override;
    bool TextureBound(std::uint32_t)override;
    DeviceResult SetTextureStageState(std::uint32_t,TextureStageRegister,std::uint32_t)override;
    std::uint32_t ReadTextureStageState(std::uint32_t,TextureStageRegister)override;
    DeviceResult SetFixedVertexLayout(std::uint32_t)override;
    DeviceResult BindBorrowedTexture(std::uint32_t,void*)override;
    DeviceResult SetTextureMatrix(std::uint32_t,const WorldMatrix&)override;
    DeviceResult BindStagingVertex(std::uint32_t,buffers::VertexBuffer*,std::uint32_t,std::uint32_t)override;
    DeviceResult BindStagingIndex(buffers::IndexBuffer*)override;
    void ReleaseStagingUploads()noexcept;
    struct DrawStats {std::uint64_t calls{},indexed{},primitives{};};
    // Optional synchronous telemetry after a successful SDK draw. The observer
    // is borrowed, cannot replace the draw/result and must not throw or mutate
    // the device. Its owner must detach it before destroying the context.
    using DrawObserver=void(*)(void* context,void* device,std::uint32_t topology,bool indexed,std::uint32_t primitives,const void* immediate,std::uint32_t stride)noexcept;
    void ObserveDraws(DrawObserver observer,void* context)noexcept{draw_observer_=observer;draw_observer_context_=context;}
    DrawStats InspectDrawStats()const noexcept{return draw_stats_;}
    void RecordDraw(bool indexed,std::uint32_t primitives,std::uint32_t topology,const void* immediate=nullptr,std::uint32_t stride=0)noexcept {
        ++draw_stats_.calls;draw_stats_.indexed+=indexed;draw_stats_.primitives+=primitives;
        if(draw_observer_)draw_observer_(draw_observer_context_,device_,topology,indexed,primitives,immediate,stride);
    }
    DeviceResult GetRenderState(StoredRenderRegister,std::uint32_t*)override;
    DeviceResult SetRenderState(StoredRenderRegister,std::uint32_t)override;
    DeviceResult GetSamplerState(std::uint32_t,StoredSamplerRegister,std::uint32_t*)override;
    DeviceResult SetSamplerState(std::uint32_t,StoredSamplerRegister,std::uint32_t)override;
    DeviceResult GetLight(std::uint32_t,StoredLight*)override;
    DeviceResult GetLightEnable(std::uint32_t,std::int32_t*)override;
    DeviceResult SetLight(std::uint32_t,const StoredLight*)override;
    DeviceResult LightEnable(std::uint32_t,std::int32_t)override;
    DeviceResult GetMaterial(StoredMaterial*)override;
    DeviceResult SetMaterial(const StoredMaterial*)override;
    void CaptureLightSlot(std::uint32_t,StoredLight*,std::int32_t*)override;
private:void* device_;std::size_t leases_{};PresentationAccess presentation_access_;
    std::shared_ptr<D3D9Uploads> uploads_;DrawStats draw_stats_;
    DrawObserver draw_observer_{};void* draw_observer_context_{};
};
}
