// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "platform_3g.hpp"
#include "CBitmap.hpp"
namespace srhd_awa::matrixgame::cursor {
// Original WinBitmap Clear/Init behavior used by MatrixCursor::Select.
// GDI handles stay opaque outside the Windows implementation.
class BitmapFrame:public CBitmap {
public:
    ~BitmapFrame(){Clear();}
    void Clear()noexcept;
    void Init();
    std::uintptr_t GetHandle()const noexcept{return bitmap_;}
private:
    std::uintptr_t bitmap_{},dc_{};
};
struct IconInfo {bool fIcon;std::uint32_t xHotspot,yHotspot;std::uintptr_t hbmMask,hbmColor;};
platform3g::IconHandle CreateIcon(const IconInfo&);
int ShowSystemCursor(bool);
bool ShowDeviceCursor(bool);
}
