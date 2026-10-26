// SPDX-License-Identifier: GPL-2.0-or-later
#include "native_loop_switch.hpp"
#include "MatrixMap.hpp"
#include "Form.hpp"
#include <switch.h>
#include <cstdio>
namespace srhd_awa::matrixgame::native_loop {
void SwitchBackend::Pump(){window_.Update();}
void SwitchBackend::Idle(){svcSleepThread(YieldType_WithoutCoreMigration);}
void SwitchBackend::Draw(CForm& form){
    // The original form skips Draw while its recovery has not recreated video
    // resources. Do not acquire a display image in that state.
    if(!(g_MatrixMap->m_Flags&MMFLAG_VIDEO_RESOURCES_READY)){form.Draw();return;}
    struct Rollback {
        platform3g::DekoDevice& device;
        ~Rollback(){const auto result=device.AbortNativeFrame();if(result)std::fprintf(stderr,"native loop frame rollback failed: %08x\n",unsigned(result));}
    } rollback{device_};
    device_.AcquireNativeFrame();
    form.Draw(); // Original Clear/BeginScene/map.Draw/EndScene/Present.
}
}
