// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "original_game.hpp"
#include "rangers_host_session.hpp"
#include "rangers_ui_context.hpp"
#include "rangers_screen_loop.hpp"
#include <memory>

namespace srhd_awa::matrixgame {
namespace platform3g { class SwitchWindow; class DekoDevice; }

// Owns the shared services of one native Rangers/MatrixGame process. The
// caller selects real ec_file game and user roots before construction and
// keeps window/device alive until after destruction. Screen/request producers
// must stop before destruction. Destruction joins workers and frees the world;
// a pending snapshot must first be restored explicitly.
// RunInitialScreens invokes the original screen dispatcher and performs
// terminal world shutdown on a normal finish. A requested runtime reload is
// returned to the caller without destroying the campaign.
class RangersIntegratedSession {
public:
    RangersIntegratedSession(platform3g::SwitchWindow&, platform3g::DekoDevice&,
                             const NativeGameOptions&, const RangersHostOptions&,
                             const RangersUiContextOptions&);
    ~RangersIntegratedSession();
    RangersScreenExit RunInitialScreens();
    // Terminal shutdown only: stop request producers first, then join the
    // request worker while the campaign and registered screens still exist.
    void StopRequestWorker();
    bool RestorePendingSnapshot();
    // Terminal shutdown after stopping the screen loop and all request
    // producers. Refuses an active detached snapshot; does not write saves.
    void ShutdownWorld();
    RangersIntegratedSession(const RangersIntegratedSession&) = delete;
    RangersIntegratedSession& operator=(const RangersIntegratedSession&) = delete;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
