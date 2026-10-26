// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace matrixgame_draw_audit {
// Telemetry boundary around a genuine original form Draw, never a replacement.
struct FrameBudget {
 FrameBudget();
 ~FrameBudget()noexcept;
 FrameBudget(const FrameBudget&)=delete;
 FrameBudget& operator=(const FrameBudget&)=delete;
};
}
