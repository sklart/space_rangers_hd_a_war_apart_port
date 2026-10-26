// SPDX-License-Identifier: GPL-2.0-or-later
#include "applet_engine_events.hpp"
#include "3g.hpp"
namespace srhd_awa::matrixgame::platform3g {
void AppletEngineEvents::FocusChanged(bool active){
    // Same engine operations and ordering as original WM_ACTIVATEAPP.
    // Native visibility is independent of focus; Win32 minimization is not used.
    if(g_Flags&GFLAG_KEEPALIVE)return;
    if(active)g_Flags|=GFLAG_APPACTIVE;else g_Flags&=~GFLAG_APPACTIVE;
    if(g_FormCur)g_FormCur->SystemEvent(active?SYSEV_ACTIVATED:SYSEV_DEACTIVATING);
}
void AppletEngineEvents::Exit()noexcept{g_Flags|=GFLAG_APPCLOSE;}
}
