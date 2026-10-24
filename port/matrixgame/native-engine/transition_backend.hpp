// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "CBitmap.hpp"
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
platform3g::WindowRect ClientRect();
std::uintptr_t CompatibleBitmap(std::uintptr_t,int,int);
std::uintptr_t CompatibleDC(std::uintptr_t);
std::uintptr_t SelectBitmap(std::uintptr_t,std::uintptr_t);
bool CopyPixels(std::uintptr_t,int,int,int,int,std::uintptr_t,int,int,std::uint32_t);
platform3g::DeviceResult Present();
}
