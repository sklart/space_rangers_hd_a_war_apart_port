// SPDX-License-Identifier: GPL-2.0-or-later
#include "platform_3g.hpp"
namespace srhd_awa::matrixgame::platform3g {
namespace {State state;}
State& Current() noexcept {return state;}
Device& ReadyDevice() {
    if(!state.ready || !state.device)throw Unsupported("3G device negotiation not complete");
    return *state.device;
}
void Deinit() noexcept {
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
