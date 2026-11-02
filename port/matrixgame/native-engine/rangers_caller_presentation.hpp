// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include <functional>
namespace srhd_awa::matrixgame {
// Owns the caller's real window phase. Capture keeps it attached for loading
// progress; EnterBattle releases it so original Init can attach the same window.
// Window/device and supplied factories must outlive this object and its lease.
class RangersCallerPresentation {
public:
 using AcquireWindow=std::function<std::unique_ptr<platform3g::WindowAttachment>()>;
 RangersCallerPresentation(platform3g::Window&,platform3g::Device&,AcquireWindow={},std::function<void()> validate_window={});
 ~RangersCallerPresentation();
 RangersCallerPresentation(const RangersCallerPresentation&)=delete;
 RangersCallerPresentation& operator=(const RangersCallerPresentation&)=delete;
 bool WindowOwned()const noexcept;
 bool BattleActive()const noexcept;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
}
