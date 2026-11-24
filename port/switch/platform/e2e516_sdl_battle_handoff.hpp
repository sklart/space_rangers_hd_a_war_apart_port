// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "e2e516_battle_handoff.hpp"
#include "renderer_platform.hpp"

namespace srhd_awa::platform::planet_battle {
// The native callbacks must create/destroy their entire battle-only window and
// device, and must be atomic on failure as required by E2e516Handoff. Register
// this provider on the Rangers UI thread for the duration of ProgramMain.
inline E2e516Handoff::Operations E2e516SdlOperations(
    std::function<void()> start_native, std::function<void()> stop_native) {
 return {renderer_platform::SuspendSdlPresentationForBattle,
         std::move(start_native), std::move(stop_native),
         renderer_platform::ResumeSdlPresentationAfterBattle};
}
}
