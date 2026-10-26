// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "native_graphics.hpp"
#include "native_map_queries.hpp"
#include "native_frame_scope.hpp"
#include "native_texture.hpp"
#include "skin_callbacks.hpp"
#include "native_cursor.hpp"
#include "native_debug_font.hpp"
#include "native_text.hpp"
#include "native_surfaces.hpp"
#include "native_stored_state.hpp"
#include "native_form_backend.hpp"
#include <functional>
struct NWindow;
namespace srhd_awa::matrixgame::platform3g {
// Borrow a real libnx window. No HWND, Win32 message dispatch, or owned close.
class SwitchWindow final:public Window,public native_cursor::Provider,public native_form::WindowOperations {
public:
    explicit SwitchWindow(NWindow&,bool owner_visible);
    ~SwitchWindow();
    SwitchWindow(const SwitchWindow&)=delete;
    void* Identity()const noexcept override;
    WindowRect ReadClientRect()override;
    WindowRect ReadWindowRect()override;
    void ResizeOuter(std::int32_t,std::int32_t)override;
    void SetFullscreenStyle()override;
    void MoveFullscreen(std::int32_t,std::int32_t)override;
    std::unique_ptr<WindowAttachment> AttachNative()override;
    std::unique_ptr<WindowAttachment> Attach(MessageHandler)override;
    void Show(Visibility)override;
    void Update()override;
    void WaitForDrawable();
    bool Drawable()const noexcept;
    std::uint32_t LastVisibilityRestoreResult()const noexcept;
    CursorHandle ExchangeCursor(CursorHandle)override;
    CursorHandle ReadCursor()override;
    void DestroyCursorIcon(IconHandle)override;
    native_cursor::Store& CursorStore()override;
    void ClientPointToScreen(Base::CPoint&)override;
    void ClipCursorToWindow()override;
    void ReleaseCursor()override;
    std::intptr_t DefaultMessage(std::uint32_t,std::uintptr_t,std::intptr_t)override;
    NWindow& NativeWindow()const noexcept{return window_;}
private:
    friend class DekoDevice;
    void BindGeometry(void*,std::function<void(std::int32_t,std::int32_t)>);
    void UnbindGeometry(void*)noexcept;
    struct LifecycleImpl;NWindow& window_;std::unique_ptr<LifecycleImpl> lifecycle_;
};
// Real device and compiled material pair. Full native Init/present remains a
// separate gate from texture-cascade capability negotiation.
class DekoDevice final:public Device,public NativeGraphics,public NativeDrawSink,public native_texture::Provider,public debug_font::native::Provider,public text_backend::native::Provider,public NativeMapQueries,public NativeFrameProvider,public NativeFrameRollback,public native_surfaces::Provider,public NativeStoredStateBackend {
public:
    using NativeStoredStateBackend::SetRenderState;
    using NativeStoredStateBackend::SetSamplerState;
    explicit DekoDevice(SwitchWindow&,std::uint32_t texture_arena_bytes=64*1024*1024);
    ~DekoDevice();
    DekoDevice(const DekoDevice&)=delete;
    DeviceCaps ReadCaps()override;
    CreationParameters ReadCreationParameters()override;
    DisplayMode ReadAdapterDisplayMode(std::uint32_t)override;
    DeviceResult CheckMultisample(std::uint32_t,std::uint32_t,PixelFormat,bool,std::uint32_t)override;
    DeviceResult ResetPresentation(Presentation&)override;
    GammaRamp ReadGamma(std::uint32_t)override;
    void RestoreGamma(std::uint32_t,const GammaRamp&)noexcept override;
    std::uint64_t AvailableTextureBytes()override;
    std::unique_ptr<DeviceStateLease> CaptureState()override;
    std::size_t OutstandingStateLeases()const override;
    std::shared_ptr<native_surfaces::Session> SurfaceSession()override;
    std::shared_ptr<native_texture::Session> TextureSession()override;
    std::shared_ptr<debug_font::native::Session> DebugFontSession()override;
    std::span<const std::byte> TextFontAsset(unsigned)override;
    WindowRect DrawableExtent()const noexcept;
    DeviceResult SetTextureStageState(std::uint32_t,TextureStageRegister,std::uint32_t)override;
    std::uint32_t ReadTextureStageState(std::uint32_t,TextureStageRegister)override;
    DeviceResult SetTextureMatrix(std::uint32_t,const WorldMatrix&)override;
    DeviceResult SetWorldTransform(const WorldMatrix&)override;
    WorldMatrix ReadWorldTransform()override;
    DeviceResult SetFixedVertexLayout(std::uint32_t)override;
    DeviceResult SetVideoViewport(const VideoViewport&)override;
    VideoViewport NativeReadViewport()const override;
    DeviceResult NativeCooperativeLevel()const override;
    std::uint32_t NativeRasterCaps()const override;
    std::uint32_t NativeStencilCaps()const override;
    DeviceResult NativeClear(std::span<const ClearRectangle>,std::uint32_t,std::uint32_t,float,std::uint32_t)override;
    DeviceResult NativeBeginScene()override;
    DeviceResult NativeEndScene()override;
    DeviceResult NativePresent()override;
    DeviceResult SetVideoMaterial(const VideoMaterial&)override;
    DeviceResult SetVideoLight(std::uint32_t,const VideoLight&)override;
    DeviceResult EnableVideoLight(std::uint32_t,bool)override;
    DeviceResult SetSamplerState(std::uint32_t,SamplerRegister,std::uint32_t)override;
    std::uint32_t ReadSamplerState(std::uint32_t,SamplerRegister)override;
    DeviceResult SetRenderState(RasterRegister,std::uint32_t)override;
    std::uint32_t ReadRenderState(RasterRegister)override;
    DeviceResult ClearTexture(std::uint32_t)override;
    bool TextureBound(std::uint32_t)override;
    DeviceResult BindBorrowedTexture(std::uint32_t,void*)override;
    DeviceResult BindStagingVertex(std::uint32_t,buffers::VertexBuffer*,std::uint32_t,std::uint32_t)override;
    DeviceResult BindStagingIndex(buffers::IndexBuffer*)override;
    DeviceResult NativeTransform(std::uint32_t,const WorldMatrix&)override;
    DeviceResult NativeDraw(std::uint32_t,std::uint32_t,std::uint32_t)override;
    DeviceResult NativeDrawUP(std::uint32_t,std::uint32_t,std::span<const std::byte>,std::uint32_t)override;
    DeviceResult NativeDrawIndexed(std::uint32_t,int,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)override;
    void SetVertexLayout(std::uint32_t)override;
    void Emit(const MeshRange&)override;
    // Bind the real primary drawable; original form owns all clear operations.
    void AcquireNativeFrame();
    void BeginNativeFrame(std::uint32_t clear_argb);
    void PresentNativeFrame();
    DeviceResult AbortNativeFrame()noexcept override;
private:
    NativeStateStore& StoredStates()override;
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
