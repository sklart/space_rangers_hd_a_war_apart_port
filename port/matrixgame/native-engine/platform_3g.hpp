// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include "../deko/sampler_state.hpp"
#include "../deko/raster_state.hpp"
#include "video_types.hpp"
namespace srhd_awa::matrixgame::platform3g {
struct Unsupported:std::runtime_error {using std::runtime_error::runtime_error;};
// Only facts actually supplied by a backend. No D3DCAPS9 impersonation or
// substitution of process RAM/staging budgets for available texture memory.
struct DeviceCaps {
    bool calibrate_gamma{};
    std::uint32_t max_texture_stages{},max_anisotropy{},swapchain_count{};
};
struct GammaRamp {std::array<std::uint16_t,256> red{},green{},blue{};};
struct LegacyDeviceHandles {std::uintptr_t api{},device{};};
using DeviceResult=std::int32_t; // Actual signed 32-bit device result; never a fake success.
inline constexpr DeviceResult DeviceSuccess=0;
struct DeviceError:std::runtime_error {
    DeviceResult result;
    DeviceError(const char* call,DeviceResult value):std::runtime_error(std::string(call)+" failed: "+std::to_string(value)),result(value){}
};
inline void CheckDeviceResult(DeviceResult result,const char* call) {
    if(result!=DeviceSuccess)throw DeviceError(call,result);
}
using WorldMatrix=std::array<float,16>; // Original row-major world transform, no transpose.
enum class TextureStageRegister:std::uint32_t {TexcoordIndex=11};
class DeviceStateLease {
public:
    virtual ~DeviceStateLease()=default;
    virtual DeviceResult Restore()noexcept=0;
};
class Device {
public:
    virtual ~Device()=default;
    virtual DeviceCaps ReadCaps()=0;
    virtual LegacyDeviceHandles ReadLegacyHandles(){throw Unsupported("legacy D3D identities unsupported by this provider");}
    virtual CreationParameters ReadCreationParameters(){throw Unsupported("backend creation parameters unsupported");}
    virtual DisplayMode ReadAdapterDisplayMode(std::uint32_t){throw Unsupported("backend display mode unsupported");}
    virtual DeviceResult CheckMultisample(std::uint32_t,std::uint32_t,PixelFormat,bool,std::uint32_t){throw Unsupported("backend multisample query unsupported");}
    // A real Reset needs a reset-safe lease, default-pool release and borrowed
    // presentation restoration. Never return success while these are unclosed.
    virtual DeviceResult ResetPresentation(Presentation&){throw Unsupported("device Reset lifecycle unsupported");}
    virtual DeviceResult SetVideoViewport(const VideoViewport&){throw Unsupported("backend video viewport unsupported");}
    virtual DeviceResult SetVideoMaterial(const VideoMaterial&){throw Unsupported("backend video material unsupported");}
    virtual DeviceResult SetVideoLight(std::uint32_t,const VideoLight&){throw Unsupported("backend video light unsupported");}
    virtual DeviceResult EnableVideoLight(std::uint32_t,bool){throw Unsupported("backend video light enable unsupported");}
    virtual GammaRamp ReadGamma(std::uint32_t swapchain)=0;
    virtual void ApplyCalibratedGamma(std::uint32_t,const GammaRamp&){throw Unsupported("backend calibrated gamma submission unsupported");}
    virtual void RestoreGamma(std::uint32_t swapchain,const GammaRamp&) noexcept=0;
    virtual std::uint64_t AvailableTextureBytes()=0;
    virtual DeviceResult SetSamplerState(std::uint32_t,SamplerRegister,std::uint32_t) {
        throw Unsupported("backend sampler state calls unsupported");
    }
    virtual std::uint32_t ReadSamplerState(std::uint32_t,SamplerRegister) {
        throw Unsupported("backend sampler state query unsupported");
    }
    virtual std::unique_ptr<DeviceStateLease> CaptureState(){throw Unsupported("backend state capture unsupported");}
    virtual std::size_t OutstandingStateLeases()const {throw Unsupported("backend state lease query unsupported");}
    virtual DeviceResult SetWorldTransform(const WorldMatrix&){throw Unsupported("backend world transform unsupported");}
    virtual WorldMatrix ReadWorldTransform(){throw Unsupported("backend world query unsupported");}
    virtual DeviceResult SetRenderState(RasterRegister,std::uint32_t){throw Unsupported("backend render state unsupported");}
    virtual std::uint32_t ReadRenderState(RasterRegister){throw Unsupported("backend render query unsupported");}
    virtual DeviceResult ClearTexture(std::uint32_t){throw Unsupported("backend texture binding unsupported");}
    virtual bool TextureBound(std::uint32_t){throw Unsupported("backend texture query unsupported");}
    virtual DeviceResult SetTextureStageState(std::uint32_t,TextureStageRegister,std::uint32_t){throw Unsupported("backend texture-stage state unsupported");}
    virtual std::uint32_t ReadTextureStageState(std::uint32_t,TextureStageRegister){throw Unsupported("backend texture-stage query unsupported");}
};
enum class Visibility {Maximized,Restored,Minimized,Normal};
struct CursorHandle {std::uintptr_t value{};bool operator==(const CursorHandle&)const=default;};
struct IconHandle {std::uintptr_t value{};};
class Window;
class TimerPeriodLease {
public:
    virtual ~TimerPeriodLease()=default;
    virtual DeviceResult Release()noexcept=0;
};
using MessageHandler=std::intptr_t(*)(Window&,std::uint32_t,std::uintptr_t,std::intptr_t);
class WindowAttachment {
public:
    virtual ~WindowAttachment()=default;
    virtual void Restore() noexcept=0;
};
class Window {
public:
    virtual ~Window()=default;
    virtual void* Identity() const noexcept=0;
    virtual std::unique_ptr<WindowAttachment> Attach(MessageHandler)=0;
    virtual void Show(Visibility)=0;
    virtual void ReleaseCursor()=0;
    virtual std::intptr_t DefaultMessage(std::uint32_t,std::uintptr_t,std::intptr_t)=0;
    virtual CursorHandle ExchangeCursor(CursorHandle){throw Unsupported("backend system cursor unsupported");}
    virtual CursorHandle ReadCursor(){throw Unsupported("backend cursor query unsupported");}
    virtual void DestroyCursorIcon(IconHandle){throw Unsupported("backend owned icon destruction unsupported");}
    virtual void Update(){throw Unsupported("backend window update unsupported");}
    virtual std::unique_ptr<TimerPeriodLease> AcquireTimerPeriod(std::uint32_t){throw Unsupported("backend timer period unsupported");}
    virtual WindowRect ReadClientRect(){throw Unsupported("backend client rectangle unsupported");}
    virtual WindowRect ReadWindowRect(){throw Unsupported("backend window rectangle unsupported");}
    virtual void ResizeOuter(std::int32_t,std::int32_t){throw Unsupported("window resize requires reset-safe presentation ownership");}
    virtual void SetFullscreenStyle(){throw Unsupported("fullscreen style requires reset-safe presentation ownership");}
    virtual void MoveFullscreen(std::int32_t,std::int32_t){throw Unsupported("fullscreen move requires reset-safe presentation ownership");}
};
struct State {
    void* module{};void* window_identity{};bool external{};
    Window* window{};Device* device{};
    std::unique_ptr<WindowAttachment> attachment;
    std::unique_ptr<TimerPeriodLease> timer;
    std::uint32_t timer_period{};
    DeviceCaps caps{};
    std::array<GammaRamp,2> saved_gamma{};
    std::array<bool,2> gamma_saved{};
    std::uint32_t flags{};
    Presentation presentation{};
    int available_texture_mib{};
    int configured_width{},configured_height{}; // Original g_ScreenX/Y settings, not a measured display mode.
    bool ready{};
};
State& Current() noexcept;
Device& ReadyDevice();
Window& ReadyWindow();
void BeginLoopTimerPeriod(std::uint32_t);
DeviceResult EndLoopTimerPeriod(std::uint32_t);
// Original DLL bootstrap and WndProc/input algorithms via typed backend calls.
// Only external/borrowed device/window mode is supported; no fake owned EXE.
void InitAsDLL(void* module,Window&,Device&);
void Deinit() noexcept;
std::intptr_t WindowProcedure(Window&,std::uint32_t,std::uintptr_t,std::intptr_t);
}
