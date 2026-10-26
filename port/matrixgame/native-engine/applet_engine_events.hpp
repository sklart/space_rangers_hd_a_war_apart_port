// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "applet_window_lifecycle.hpp"
namespace srhd_awa::matrixgame::platform3g {
class AppletEngineEvents final:public applet_window::Observer {
public:
    void FocusChanged(bool)override;
    void Exit()noexcept override;
};
}
