// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
#include <functional>
#include <utility>
namespace srhd_awa::matrixgame {
namespace platform3g {class SwitchWindow;}
struct RangersNativeInputBackend {
 std::function<void()> poll,wait_for_drawable,reset_input;
 std::function<bool()> drawable;
 std::function<std::pair<int,int>()> extent;
 std::function<void(int,int)> warp;
};
// Host/UiContext and the attached caller window outlive this owner. Original
// UI/script/campaign users are destroyed first. No SDL or HWND is created.
class RangersNativeInput {
public:
 explicit RangersNativeInput(RangersNativeInputBackend);
 ~RangersNativeInput();
 RangersNativeInput(const RangersNativeInput&)=delete;RangersNativeInput& operator=(const RangersNativeInput&)=delete;
private:struct Impl;std::unique_ptr<Impl> impl_;
};
#ifdef __SWITCH__
RangersNativeInputBackend MakeSwitchRangersInputBackend(platform3g::SwitchWindow&);
#endif
}
