// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef _WIN32
#include <windows.h>
#endif
#include "native_form_backend.hpp"
namespace srhd_awa::matrixgame::native_form {
void ClientPointToScreen(Base::CPoint& point){
    auto& window=platform3g::ReadyWindow();
    if(auto* native=dynamic_cast<WindowOperations*>(&window)){native->ClientPointToScreen(point);return;}
#ifdef _WIN32
    POINT value{point.x,point.y};
    if(!::ClientToScreen(static_cast<HWND>(window.Identity()),&value))throw std::runtime_error("form ClientToScreen failed");
    point.x=value.x;point.y=value.y;
#else
    throw platform3g::Unsupported("form client-to-screen provider unavailable");
#endif
}
void ClipCursorToWindow(){
    auto& window=platform3g::ReadyWindow();
    if(auto* native=dynamic_cast<WindowOperations*>(&window)){native->ClipCursorToWindow();return;}
#ifdef _WIN32
    const auto r=window.ReadWindowRect();const RECT bounds{r.left,r.top,r.right,r.bottom};
    if(!::ClipCursor(&bounds))throw std::runtime_error("form ClipCursor failed");
#else
    throw platform3g::Unsupported("form cursor clipping provider unavailable");
#endif
}
[[noreturn]] void Screenshot(){throw platform3g::Unsupported("CFormMatrixGame::Keyboard F9: native screenshot provider unavailable");}
}
