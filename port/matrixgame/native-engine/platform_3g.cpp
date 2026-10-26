// SPDX-License-Identifier: GPL-2.0-or-later
#include "platform_3g.hpp"
#include "pick_viewport.hpp"
namespace srhd_awa::matrixgame::platform3g {
namespace {State state;}
State& Current() noexcept {return state;}
Device& ReadyDevice() {
    if(!state.ready || !state.device)throw Unsupported("3G device negotiation not complete");
    return *state.device;
}
Window& ReadyWindow() {
    if(!state.ready || !state.window)throw Unsupported("3G window negotiation not complete");
    return *state.window;
}
void InitAsNative(Window& window,Device& device) {
    if(state.ready || state.attachment)throw std::logic_error("native platform already attached");
    try {
        state.window=&window;state.window_identity=window.Identity();state.device=&device;state.external=true;
        state.attachment=window.AttachNative();
        if(!state.attachment)throw std::logic_error("native window returned no lifetime attachment");
        state.caps=device.ReadCaps(); // Implemented material paths, never guessed D3D caps.
        state.available_texture_mib=static_cast<int>(device.AvailableTextureBytes()/(1024*1024));
        if(state.caps.calibrate_gamma)throw Unsupported("native display gamma negotiation requires explicit implementation");
        state.ready=true;
    }catch(...){Deinit();throw;}
}
void BeginLoopTimerPeriod(std::uint32_t period) {
    auto& window=ReadyWindow();
    if(state.timer)throw std::logic_error("loop timer period already owned");
    auto lease=window.AcquireTimerPeriod(period);
    if(!lease)throw std::logic_error("backend returned no timer period lease");
    state.timer=std::move(lease);state.timer_period=period;
}
DeviceResult EndLoopTimerPeriod(std::uint32_t period) {
    if(!state.timer)return DeviceSuccess; // Idempotent cleanup, no OS call claimed.
    if(period!=state.timer_period)throw std::logic_error("loop timer period differs from owned request");
    const auto result=state.timer->Release();state.timer.reset();state.timer_period=0;
    return result;
}
void Deinit() noexcept {
    state.timer.reset();state.timer_period=0; // Balanced even after loop failure.
    // DLL/device objects are borrowed, as in the original L3GDeinit; no Release
    // or DestroyWindow is performed on somebody else's device/window.
    if(state.device)for(std::uint32_t i=0;i<state.gamma_saved.size();++i)
        if(state.gamma_saved[i])state.device->RestoreGamma(i,state.saved_gamma[i]);
    if(state.attachment)state.attachment->Restore();
    const auto flags=state.flags & ~(1u<<5);
    const auto width=state.configured_width,height=state.configured_height;
    const auto available=state.available_texture_mib;
    state=State{};
    state.flags=flags; // Run reads EXITLOOP after SafeFree; retain non-device flags.
    state.configured_width=width;state.configured_height=height;
    state.available_texture_mib=available; // Original Deinit retains these last configured/queried scalar values.
}
}
namespace srhd_awa::matrixgame {
PickViewport ReadPickViewport() noexcept {
    const auto& state=platform3g::Current();
    return {state.configured_width,state.configured_height};
}
}
