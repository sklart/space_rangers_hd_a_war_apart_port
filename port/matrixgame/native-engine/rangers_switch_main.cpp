// SPDX-License-Identifier: GPL-2.0-or-later
// Integrated application entry. Runtime reload and hardware acceptance remain
// open; do not publish this build as a completed port.
#include "rangers_integrated_session.hpp"
#include "platform_switch.hpp"
#include "ec_file_adapter.hpp"
#include "user_root.hpp"
#include "rangers_session_log.hpp"
#include <switch.h>
#include <cstdio>
#include <exception>

int main() {
    namespace mg=srhd_awa::matrixgame;
    namespace p=mg::platform3g;
    try {
        if(appletGetAppletType()!=AppletType_Application && appletGetAppletType()!=AppletType_SystemApplication)
            throw p::Unsupported("integrated game requires application mode");
        if(!serviceIsActive(hidGetServiceSession()) || !hidGetSharedmemAddr())
            throw p::Unsupported("integrated game requires libnx HID");
        hidInitializeNpad();
        const HidNpadIdType ids[]{HidNpadIdType_No1,HidNpadIdType_Handheld};
        p::CheckDeviceResult(static_cast<p::DeviceResult>(hidSetSupportedNpadIdType(ids,2)),"Npad player IDs");
        p::CheckDeviceResult(static_cast<p::DeviceResult>(hidSetSupportedNpadStyleSet(HidNpadStyleSet_NpadFullCtrl)),"Npad controller styles");
        p::CheckDeviceResult(static_cast<p::DeviceResult>(appletSetAppletWindowVisibility(true)),"applet visibility");
        constexpr auto game="sdmc:/switch/space-rangers-hd-a-war-apart/game";
#ifdef MATRIXGAME_ISOLATED_TEST
        constexpr auto user="sdmc:/switch/space-rangers-hd-a-war-apart/integrated-test";
#else
        constexpr auto user="sdmc:/switch/space-rangers-hd-a-war-apart";
#endif
        srhd_awa::platform::ec_file::SetGameRoot(game);
        srhd_awa::platform::ec_file::SetUserRoot(user);
        if(!srhd_awa::platform::user_root::EnsureLayout())
            throw p::Unsupported("cannot create user directory layout");
#ifdef MATRIXGAME_ISOLATED_TEST
        if(!std::freopen("sdmc:/switch/space-rangers-hd-a-war-apart/integrated-test/logs/stdout.log","a",stdout) ||
           !std::freopen("sdmc:/switch/space-rangers-hd-a-war-apart/integrated-test/logs/stderr.log","a",stderr))
            throw p::Unsupported("cannot open isolated test logs");
        std::setvbuf(stdout,nullptr,_IONBF,0);std::setvbuf(stderr,nullptr,_IONBF,0);
        std::puts("integrated test: startup");
        constexpr auto game_log="sdmc:/switch/space-rangers-hd-a-war-apart/integrated-test/logs/game.log";
#else
        constexpr auto game_log="sdmc:/switch/space-rangers-hd-a-war-apart/logs/game.log";
#endif
        mg::InitializeRangersLog(game_log);
        p::SwitchDisplay display;
        p::SwitchWindow window(display.NativeWindow(),true,&display);
        p::DekoDevice device(window);
        mg::NativeGameOptions options{window,device};
        options.draw_sink=&device;
        options.resource_root=L"sdmc:/switch/space-rangers-hd-a-war-apart/game";
#ifdef MATRIXGAME_ISOLATED_TEST
        options.output_root=L"sdmc:/switch/space-rangers-hd-a-war-apart/integrated-test/runtime/matrixgame";
#else
        options.output_root=L"sdmc:/switch/space-rangers-hd-a-war-apart/runtime/matrixgame";
#endif
        const auto drawable=window.ReadClientRect();
        mg::RangersUiContextOptions ui;
        ui.width=ui.presentation_width=drawable.right-drawable.left;
        ui.height=ui.presentation_height=drawable.bottom-drawable.top;
        std::printf("integrated test: UI drawable %dx%d\n",ui.width,ui.height);
        mg::RangersIntegratedSession session(window,device,options,{},ui);
        const auto result=session.RunInitialScreens();
        std::printf("integrated test: screen loop returned %d\n",static_cast<int>(result));
        if(result==mg::RangersScreenExit::RuntimeReloadRequested) {
            std::fprintf(stderr,"integrated runtime reload is not yet supported\n");
            return 2;
        }
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"integrated game stopped: %s\n",e.what());
        return 1;
    }
}
