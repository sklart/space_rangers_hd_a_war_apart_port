// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "native_loop.hpp"
#include "platform_switch.hpp"
#include <chrono>
namespace srhd_awa::matrixgame::native_loop {
class SwitchBackend final:public Backend {
public:
    SwitchBackend(platform3g::SwitchWindow& window,platform3g::DekoDevice& device):window_(window),device_(device){}
    void Pump()override;void Idle()override;void Draw(CForm&)override;
private:
    platform3g::SwitchWindow& window_;platform3g::DekoDevice& device_;
    std::chrono::steady_clock::time_point report_start_{};
    platform3g::DekoDevice::DiagnosticCounters report_base_{};
    std::uint64_t report_frames_{};
};
}
