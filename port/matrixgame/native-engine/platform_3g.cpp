// SPDX-License-Identifier: GPL-2.0-or-later
#include "platform_3g.hpp"
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
    state=State{};
    state.flags=flags; // Run reads EXITLOOP after SafeFree; retain non-device flags.
}
}
