// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
class CBitmap;
namespace srhd_awa::matrixgame::native_form {
enum class ScreenshotFormat {Rgba8,Rgb565};
struct ScreenshotPixels {
    unsigned width{},height{},pitch{};
    ScreenshotFormat format{ScreenshotFormat::Rgba8};
    std::vector<std::byte> bytes;
};
class ScreenshotProvider {
public:
    virtual ~ScreenshotProvider()=default;
    virtual ScreenshotPixels CaptureScreenshot()=0;
};
// A completed scene remains valid only until recording/reset/abort can alter it.
// The slot is published after successful Present and queue completion.
class ScreenshotFrame {
    int slot_{-1};
public:
    void Invalidate()noexcept{slot_=-1;}
    void Presented(int slot,unsigned count){
        if(slot<0 || unsigned(slot)>=count){Invalidate();throw std::invalid_argument("screenshot presented slot");}
        slot_=slot;
    }
    int Require(bool busy,bool healthy)const {
        if(busy || !healthy || slot_<0)throw std::logic_error("screenshot requires completed healthy idle frame");
        return slot_;
    }
};
// Real original CBitmap PNG/CFile path; requires the explicit native output scope.
void ValidateScreenshotPixels(const ScreenshotPixels&);
void CopyScreenshotPixels(CBitmap&,const ScreenshotPixels&);
std::wstring SaveScreenshot(const ScreenshotPixels&);
std::wstring Screenshot();
}
