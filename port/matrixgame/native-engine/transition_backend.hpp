// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "CBitmap.hpp"
#include "native_screenshot.hpp"
namespace srhd_awa::matrixgame::transition {
class CaptureDC {
public:
    CaptureDC();~CaptureDC(){Release();}
    CaptureDC(const CaptureDC&)=delete;
    CaptureDC& operator=(const CaptureDC&)=delete;
    void Release()noexcept;
    operator std::uintptr_t()const noexcept{return dc_;}
private:std::uintptr_t window_{},dc_{};
};
class Bitmap:public CBitmap {
public:
    ~Bitmap(){Clear();}
    void Clear()noexcept;
    void Save(bool);
    std::uintptr_t GetHandle()const noexcept{return bitmap_;}
    std::uintptr_t GetDC()const noexcept{return dc_;}
    void SetHandle(std::uintptr_t value)noexcept{bitmap_=value;}
    void SetDC(std::uintptr_t value)noexcept{dc_=value;}
private:std::uintptr_t bitmap_{},dc_{};
};
// Own real completed-frame pixels before Init/reset invalidates their GPU slot.
// One original loading transition consumes the snapshot; no stale-frame reuse.
class LoadingCaptureScope {
public:
    explicit LoadingCaptureScope(native_form::ScreenshotProvider&);
    ~LoadingCaptureScope();
    LoadingCaptureScope(const LoadingCaptureScope&)=delete;
    LoadingCaptureScope& operator=(const LoadingCaptureScope&)=delete;
private:
    friend void CaptureLoadingImage(Bitmap&,int,int);
    native_form::ScreenshotPixels image_;
    bool consumed_{};
};
void CaptureLoadingImage(Bitmap&,int width,int height);
platform3g::WindowRect ClientRect();
std::uintptr_t CompatibleBitmap(std::uintptr_t,int,int);
std::uintptr_t CompatibleDC(std::uintptr_t);
std::uintptr_t SelectBitmap(std::uintptr_t,std::uintptr_t);
bool CopyPixels(std::uintptr_t,int,int,int,int,std::uintptr_t,int,int,std::uint32_t);
platform3g::DeviceResult Present();
}
