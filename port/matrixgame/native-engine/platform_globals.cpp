// SPDX-License-Identifier: GPL-2.0-or-later
#include "platform_3g.hpp"
// Same mutable lvalues seen by original engine call sites and typed WndProc.
// No SDK interface/caps object is fabricated or published here.
void*& g_HInst=srhd_awa::matrixgame::platform3g::Current().module;
void*& g_Wnd=srhd_awa::matrixgame::platform3g::Current().window_identity;
std::uint32_t& g_Flags=srhd_awa::matrixgame::platform3g::Current().flags;
int& g_ScreenX=srhd_awa::matrixgame::platform3g::Current().configured_width;
int& g_ScreenY=srhd_awa::matrixgame::platform3g::Current().configured_height;
int& g_AvailableTexMem=srhd_awa::matrixgame::platform3g::Current().available_texture_mib;
